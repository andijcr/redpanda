#include "cloud_storage/types.h"
#include "reflection/arity.h"
#include "serde/envelope.h"
#include "utils/delta_for.h"

#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <boost/test/unit_test.hpp>

namespace cloud_storage {

struct cstore_v2
  : public boost::stl_interfaces::sequence_container_interface<cstore_v2> {
    // NOTEANDREA: static assert that segment_meta is not an envelope, we need
    // this to extract the correct tuple of fields (check)

    // number of fields in a segment meta
    constexpr static auto sm_num_fields = reflection::arity<segment_meta>();
    // which field is the base_offset. this gets a special treatment as it's the
    // key for this map
    constexpr static auto sm_base_offset_position = 2u;
    constexpr static auto sm_committed_offset_position = 3u;

    // delta_alg, encoder type and decoder type for each fields. they all get a
    // delta_xor compression, except the key field (base_offset) that gets a
    // delta_delta encoder, (it's monotonic)
    template<size_t idx>
    using delta_alg_t = std::conditional_t<
      idx == sm_base_offset_position,
      details::delta_delta<int64_t>,
      details::delta_xor>;
    template<size_t idx>
    using encoder_t = deltafor_encoder<int64_t, delta_alg_t<idx>, true>;
    template<size_t idx>
    using decoder_t = deltafor_decoder<int64_t, delta_alg_t<idx>>;

    // tuple-type with a deltafor_encoder for each field in a segment_meta.
    using encoder_tuple_t = decltype([]<size_t... Is>(
                                       std::index_sequence<Is...>) {
        return std::tuple{encoder_t<Is>{}...};
    }(std::make_index_sequence<sm_num_fields>()));
    using encoder_in_buffer = std::array<int64_t, details::FOR_buffer_depth>;
    // for each key (the base offset of segment_meta), store the position of
    // each field in the corresponding deltafor_buffer

    using decoder_tuple_t = decltype([]<size_t... Is>(
                                       std::index_sequence<Is...>) {
        return std::tuple{decoder_t<Is>{}...};
    }(std::make_index_sequence<sm_num_fields>()));

    struct hint_t {
        model::offset key;
        std::array<size_t, sm_num_fields> mapped_value;
    };

private:
    // NOTEANDREA move the state to a ss::lw_shared_ptr managed struct, to
    // create a copy-on-write structure

    struct w_state;
    struct r_state {
        // parent is used to implement copy-on-write, since _frame_fields share
        // buffers with _parent
        ss::lw_shared_ptr<const w_state> _parent;
        decoder_tuple_t _frame_fields{};
        auto empty() const { return std::get<0>(_frame_fields).empty(); }

        auto clone() const -> r_state {
            if (empty()) {
                return {};
            }
            // get a pristine r_state
            auto res = _parent->get_r_state();
            // fix up the _frame_fields by skipping ahead
            [&]<size_t... Is>(std::index_sequence<Is...>) {
                // get a copy of all the skip info
                auto pos_tuple = std::tuple{
                  std::get<Is>(_frame_fields)
                    .get_pos()
                    .to_stream_pos_t(std::get<Is>(_parent->_frame_fields)
                                       .get_position()
                                       .offset)...};
                // apply them
                (std::get<Is>(res._frame_fields).skip(std::get<Is>(pos_tuple)),
                 ...);
            }(std::make_index_sequence<std::tuple_size_v<decoder_tuple_t>>());

            return res;
        }
    };
    struct w_state : public ss::enable_lw_shared_from_this<w_state> {
        // NOTEANDREA should be possible to have an upper bound and move this to
        // std::array invariants: this is sorted on key, has few elements, and
        // key in contained in the range of base_offsets covered by this span
        // this makes it so that on base_offset lookup we can linearly (or
        // binary) search this sequence for it (or the bigger value no bigger
        // than it) in the sequence, and it will give us an array of offsets
        // into the deltafor buffer, to speedup retrieval of encoded
        // segment_meta
        std::vector<hint_t> _frame_hints{};

        // the deltafor encoders are contained here. invariants: all the
        // deltafor have the same size, together they encode all the
        // segment_meta saved in this frame
        encoder_tuple_t _frame_fields{};

        // used to implement copy-on-write: const_iterator grab a shared_ptr, if
        // some destructive operation (truncate, rewrite, append) has to happen
        // and has_reader() is true, a copy is created
        inline bool has_reader() const { return use_count() > 1; }

        friend bool
        operator==(w_state const& lhs, w_state const& rhs) noexcept {
            // only frame fields contains data that matters, _frame_hints are a
            // lookup optimization
            return lhs._frame_fields == rhs._frame_fields;
        }

        auto get_base_offset() const -> model::offset {
            // precondition: this is not empty
            return model::offset(
              std::get<sm_base_offset_position>(_frame_fields)
                .get_initial_value());
        }

        auto get_commited_offset() const -> model::offset {
            // precondition: this is not empty
            return model::offset(
              std::get<sm_committed_offset_position>(_frame_fields)
                .get_last_value());
        }

        auto get_r_state() const -> r_state {
            return {
              ._parent = shared_from_this(),
              ._frame_fields =
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return decoder_tuple_t{decoder_t<Is>{
                      std::get<Is>(_frame_fields).get_initial_value(),
                      std::get<Is>(_frame_fields).get_row_count(),
                      std::get<Is>(_frame_fields).share(),
                      delta_alg_t<Is>{}}...};
                }(std::make_index_sequence<
                  std::tuple_size_v<encoder_tuple_t>>())};
        };
    };

    // invariant: if _size is > details::FOR_buffer_depth, _encoded_state is not
    // empty and no w_state is ever empty
    std::vector<ss::lw_shared_ptr<w_state>> _encoded_state{};
    // before they can encoded in the encoders, we need to accumulate
    // #details::FOR_buffer_depth segment_meta objects. this in_buffer contains
    // 1 less then that. when there is a request to append the last one, they
    // will all be columnar-projected and encoded in the encoders, so this
    // buffer will never fill to full capacity
    std::array<segment_meta, details::FOR_buffer_depth - 1> _in_buffer{};

    // size doubles down as an index in _in_buffer, since _frame_fields contains
    // N*details::FOR_buffer_depth elements

    std::size_t _size{0};

public:
    // returns base and committed offset covered by this span, as the base
    // offset of first element and commit offset of the last one
    auto get_base_offset() const noexcept -> model::offset;
    auto get_committed_offset() const noexcept -> model::offset;

    // maybe there is no need to use a proxy_iterator, since he reference will
    // remain valid as long no operation++ happens. the operator++ happening on
    // a shared _state will not invalidate ours
    struct const_frame_iterator final
      : public boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta> {
        using base_type = boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta>;
        constexpr const_frame_iterator() = default;
        explicit const_frame_iterator(cstore_v2 const& cf);
        constexpr segment_meta const& operator*() const noexcept {
            vassert(_state, "invariant: _state is valid if this is not end");
            return _state->get();
        }
        constexpr const_frame_iterator& operator++() noexcept;
        friend constexpr bool operator==(
          const_frame_iterator const& lhs,
          const_frame_iterator const& rhs) noexcept {
            return lhs._pseudo_base_offset == rhs._pseudo_base_offset;
        }
        using base_type::operator++;

    private:
        // this is meant to be called from a non const method to ensure that a
        // destructive op does not touch other shared state
        void ensure_owned();
        // if this iterator is not end, this value is the actual base_offset of
        // the pointed segment_meta if this iterator is end, this value is the
        // sentinel value model::offset, and this an invariant of the iterator:
        // if the data pointed to is exhausted, this value is set to
        // model::offset
        model::offset _pseudo_base_offset{};
        // state is expensive, since this is a const_iterator it makes sense to
        // store it in a shared pointer with a copy_on_write mechanism
        struct state {
            std::array<segment_meta, details::FOR_buffer_depth>
              _uncompressed_head{};
            r_state _active_frame{};
            // invariant: size() means _uncompressed_head is empty and needs to
            // be decompressed
            uint8_t _head_ptr = _uncompressed_head.size();
            uint8_t _trailing_ptr = {0};
            uint8_t _trailing_sz;
            std::array<segment_meta, details::FOR_buffer_depth - 1>
              _trailing_sm;

            std::vector<ss::lw_shared_ptr<const w_state>> _frames_to_read;

            explicit state(cstore_v2 const& cs)
              : _trailing_sz{uint8_t(cs.size() % details::FOR_buffer_depth)}
              , _trailing_sm{cs._in_buffer}
              , _frames_to_read(
                  cs._encoded_state.begin(), cs._encoded_state.end()) {}

            // let's be explicit: clone or share, no implicit copy op allowed
            state(state const&) = delete;
            state& operator=(state const&) = delete;
            // move op are allowed
            state(state&& rhs) noexcept = default;
            state& operator=(state&&) noexcept = default;

            // clone performs a "deep" clone such as destructive op on this do
            // not modify the returned object
            auto clone() const -> state;

            auto get() const -> segment_meta const& {
                vassert(
                  (_head_ptr < _uncompressed_head.size() && _trailing_ptr == 0)
                    || (_head_ptr == _uncompressed_head.size() && _trailing_ptr < _trailing_sz),
                  "ensure invariants about the two buffers: that the either "
                  "the head is being consumed or that the trailer is begin "
                  "consumed (without being terminaned, since that would be "
                  "like accessing end())");
                if (_head_ptr < _uncompressed_head.size()) {
                    return _uncompressed_head[_head_ptr];
                }

                return _trailing_sm[_trailing_ptr];
            }

        private:
            state() = default;
        };
        // invariant: _state is valid if this is not end pointer
        ss::lw_shared_ptr<state> _state{};
    };

    BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
      const_frame_iterator, std::forward_iterator);

    // these are for sequence_container_interface to do its magic
    using value_type = segment_meta;
    using reference = segment_meta&;
    using iterator = const_frame_iterator;
    using const_iterator = const_frame_iterator;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;

    cstore_v2() = default;
    cstore_v2(cstore_v2 const&);
    cstore_v2& operator=(cstore_v2 const&);
    cstore_v2(cstore_v2&&);
    cstore_v2& operator=(cstore_v2&&);

    inline const_frame_iterator begin() {
        return empty() ? const_frame_iterator{} : const_frame_iterator{*this};
    }
    inline const_frame_iterator end() { return {}; }
    void swap(cstore_v2& rhs) noexcept;
    size_t max_size() const;
    // these could be inherited by sequence_container but we can do better
    constexpr size_t size() const noexcept { return _size; }
    bool operator==(cstore_v2 const& rhs) const noexcept;

    // providing these offers some other possibilities
    cstore_v2(const_iterator const&, const_iterator const&);
};

void cstore_v2::swap(cstore_v2& rhs) noexcept {
    if (this == &rhs) {
        return;
    }
    auto us = std::tie(_encoded_state, _in_buffer, _size);
    auto them = std::tie(rhs._encoded_state, rhs._in_buffer, rhs._size);
    std::swap(us, them);
}

bool cstore_v2::operator==(cstore_v2 const& rhs) const noexcept {
    // exclude hints, as they are just a lookup optimization. compare fields in
    // order of simplicity
    constexpr static auto to_reference = [](auto& ptr) -> decltype(auto) {
        return *ptr;
    };
    return std::tie(_size, _in_buffer) == std::tie(rhs._size, rhs._in_buffer)
           && std::ranges::equal(
             _encoded_state,
             rhs._encoded_state,
             std::equal_to<>{},
             to_reference,
             to_reference);
}

auto cstore_v2::get_base_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return model::offset{};
    }
    if (_size <= _in_buffer.size()) {
        // no data in the encoders yet
        return _in_buffer[0].base_offset;
    }

    // data in the encoders
    return _encoded_state.front()->get_base_offset();
}
auto cstore_v2::get_committed_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return model::offset{};
    }

    if (_size <= _in_buffer.size()) {
        // no data in the encoders yet
        return _in_buffer[_size - 1].committed_offset;
    }

    return _encoded_state.back()->get_commited_offset();
}
cstore_v2::const_frame_iterator::const_frame_iterator(cstore_v2 const& cf)
  : _pseudo_base_offset(cf.get_base_offset())
  , _state(ss::make_lw_shared<state>(cf)) {}

auto cstore_v2::const_frame_iterator::state::clone() const -> state {
    auto res = state{};
    res._head_ptr = _head_ptr;
    // just copy forward data
    for (auto i = size_t(_head_ptr); i < _uncompressed_head.size(); ++i) {
        res._uncompressed_head[i] = _uncompressed_head[i];
    }
    res._trailing_ptr = _trailing_ptr;
    res._trailing_sz = _trailing_sz;
    for (auto i = size_t(_trailing_ptr); i < _trailing_sz; ++i) {
        res._trailing_sm[i] = _trailing_sm[i];
    }

    // create a clone from the original cstore iobuf
    res._active_frame = _active_frame.clone();
    // copy the frames after this that will be read after the active
    // frame
    res._frames_to_read = _frames_to_read;
    return res;
}
void cstore_v2::const_frame_iterator::ensure_owned() {
    if (!_state.owned()) {
        _state = ss::make_lw_shared(_state->clone());
    }
}
} // namespace cloud_storage

BOOST_AUTO_TEST_SUITE(test_suite_cstore_v2);

BOOST_AUTO_TEST_CASE(hello_world) {}

BOOST_AUTO_TEST_SUITE_END();
