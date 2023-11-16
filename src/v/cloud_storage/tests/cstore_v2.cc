#include "cloud_storage/types.h"
#include "reflection/arity.h"
#include "serde/envelope.h"
#include "utils/delta_for.h"

#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <boost/test/unit_test.hpp>

#include <ranges>

namespace {

template<
  size_t Sz,
  std::invocable<std::integral_constant<std::size_t, 0>> Callable>
auto tuple_for(
  Callable&& functor, std::integral_constant<std::size_t, Sz> Size = {}) {
    constexpr static auto functor_is_void = std::is_void_v<
      std::invoke_result_t<Callable, std::integral_constant<std::size_t, 0>>>;
    return [&]<auto... Is>(std::index_sequence<Is...>) {
        if constexpr (functor_is_void) {
            return (
              std::invoke(functor, std::integral_constant<std::size_t, Is>{}),
              ...);
        } else {
            return std::tuple{std::invoke(
              functor, std::integral_constant<std::size_t, Is>{})...};
        }
    }(std::make_index_sequence<Size>());
}

} // namespace
namespace cloud_storage {

struct cstore_v2
  : public boost::stl_interfaces::sequence_container_interface<cstore_v2> {
    // NOTEANDREA: static assert that segment_meta is not an envelope, we need
    // this to extract the correct tuple of fields (check)

    // number of fields in a segment meta
    constexpr static auto sm_num_fields = reflection::arity<segment_meta>();
    using sm_num_fields_t = std::integral_constant<std::size_t, sm_num_fields>;
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
    using encoder_tuple_t = decltype(tuple_for<sm_num_fields>(
      [](auto ix) { return encoder_t<ix>{}; }));

    using encoder_in_buffer = std::array<int64_t, details::FOR_buffer_depth>;
    // for each key (the base offset of segment_meta), store the position of
    // each field in the corresponding deltafor_buffer

    // tuple-type with a deltafor_decoder for each field in a segment_meta.
    using decoder_tuple_t = decltype(tuple_for<sm_num_fields>(
      [](auto ix) { return decoder_t<ix>{}; }));

    struct hint_t {
        model::offset key;
        std::array<deltafor_stream_pos_t<int64_t>, sm_num_fields> mapped_value;
    };

    struct segment_range {
        model::offset base_offset;
        model::offset committed_offset;
        segment_range() noexcept = default;
        segment_range(model::offset base, model::offset committed) noexcept: base_offset{base}, committed_offset{committed} {}
        explicit segment_range(segment_meta const& sm) noexcept: base_offset{sm.base_offset}, committed_offset{sm.committed_offset} {}
        
        constexpr auto is_replaced_by(segment_range const& sr) const {
            // sm starts early and ends later than this
            return sr.base_offset <= base_offset
                   && sr.committed_offset >= committed_offset;
        }
        constexpr auto
        is_replaced_by(cloud_storage::segment_meta const& sm) const {
            // sm starts early and ends later than this
            return sm.base_offset <= base_offset
                   && sm.committed_offset >= committed_offset;
        }
    };

private:
    // NOTEANDREA move the state to a ss::lw_shared_ptr managed struct, to
    // create a copy-on-write structure

    struct w_state;

    // struct used by const_iterator to read from a w_state.
    struct r_state {
        // parent is used to implement copy-on-write, since _frame_fields share
        // buffers with _parent
        ss::lw_shared_ptr<const w_state> _parent{};
        // the compressed data is read through these
        decoder_tuple_t _frame_fields{};
        // true: no more data can be uncompressed
        auto empty() const noexcept -> bool;
        // create a clone of r_state such that this->uncompress does not modify
        // the state of the new r_state
        auto clone() const -> r_state;
        // uncompress the next batch of segment_meta. modifies _frame_fields
        void uncompress(std::span<segment_meta, details::FOR_buffer_depth> out);
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

        w_state() = default;
        // disable public copy and move since get_w_state generates back-links
        // to this object. use clone() and get_r_state() instead
    private:
        struct lw_copy_t {};
        w_state(w_state const& lhs)
          : _frame_hints{lhs._frame_hints}
          , _frame_fields{tuple_for<sm_num_fields>([&](auto ix) {
              auto& lhs_enc = std::get<ix>(lhs._frame_fields);
              return encoder_t<ix>{
                lhs_enc.get_initial_value(),
                lhs_enc.get_row_count(),
                lhs_enc.get_last_value(),
                lhs_enc.copy()};
          })} {}
        w_state& operator=(w_state const& oth) {
            if (this != &oth) {
                auto cpy = oth;
                *this = std::move(cpy);
            }
            return *this;
        };
        w_state(w_state&&) = default;
        w_state& operator=(w_state&&) = default;
        w_state(w_state& lhs, lw_copy_t)
          : _frame_hints{lhs._frame_hints}
          , _frame_fields{tuple_for<sm_num_fields>([&](auto ix) {
              return encoder_t<ix>(&std::get<ix>(lhs._frame_fields));
          })} {}

    public:
        ~w_state() = default;

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

        auto get_last_range() const noexcept -> segment_range {
            // precondition: this is not empty;
            return segment_range{
              model::offset(std::get<sm_base_offset_position>(_frame_fields)
                              .get_last_value()),
              model::offset(
                std::get<sm_committed_offset_position>(_frame_fields)
                  .get_last_value())};
        }

        // create a r_state for this w_state. modifications to this could
        // reflect on w_state, that's why the resulting r_state has a back-link
        // to this, to implement copy-on-write
        // optionally, if hint_p is not nullptr, apply the hint to the r_state
        // to skip ahead
        auto get_r_state(hint_t const* hint_p = nullptr) const -> r_state;

        // create a clone of this w_state, such that non-const op on the
        // returned object do not affect this
        auto clone() const -> w_state {
            auto res = w_state{};

            return res;
        }
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

    constexpr auto in_buffer_size() const noexcept -> std::size_t {
        return size() % details::FOR_buffer_depth;
    }

    constexpr auto in_buffer_view() const noexcept
      -> std::span<const segment_meta> {
        return std::span{_in_buffer}.first(in_buffer_size());
    }

    // returns base and committed offset covered by this span, as the base
    // offset of first element and commit offset of the last one
    auto get_base_offset() const noexcept -> model::offset;
    auto get_committed_offset() const noexcept -> model::offset;

public:
    auto get_range() const noexcept -> segment_range {
        return {get_base_offset(), get_committed_offset()};
    }

    auto get_last_segment_range() const noexcept -> segment_range {
        if (empty()) {
            return {};
        }

        if (size() < _in_buffer.max_size()) {
            // still in the buffer
            auto& last = _in_buffer[size() - 1];
            return {last.base_offset, last.committed_offset};
        }

        return _encoded_state.back()->get_last_range();
    }
    // this is a proxy iterator because references returned by this will not be
    // valid once the iterator is advanced
    struct const_frame_iterator final
      : public boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta> {
        using base_type = boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta>;

        // works as end iterator
        constexpr const_frame_iterator() = default;
        // create an iterator with data
        template<typename Rng>
        explicit const_frame_iterator(
          std::span<const segment_meta> trailing_buf,
          Rng&& encoded_frames_range);
        // create and iterator with data and an optional hint for the first
        // buffer (nullptr of emtpy)
        template<typename Rng>
        explicit const_frame_iterator(
          std::span<const segment_meta> trailing_buf,
          Rng&& encoded_frames_range,
          hint_t const* maybe_hint);

        // copy and move works as expected, sharing the lw_shared_ptr _state

        auto operator*() const noexcept -> segment_meta const&;
        auto operator++() noexcept -> const_frame_iterator&;

        friend constexpr bool operator==(
          const_frame_iterator const& lhs,
          const_frame_iterator const& rhs) noexcept {
            // get base_offset or default if end iterator
            auto lhs_base_off = lhs._state ? lhs._state->get().base_offset
                                           : model::offset();
            auto rhs_base_off = rhs._state ? rhs._state->get().base_offset
                                           : model::offset();
            return lhs_base_off == rhs_base_off;
        }
        using base_type::operator++;

    private:
        // this is meant to be called from a non const method to ensure that a
        // destructive op does not touch other shared state
        void ensure_owned();

        // state is expensive, since this is a const_iterator it makes sense to
        // store it in a shared pointer with a copy_on_write mechanism
        struct state {
            // array to contain data as we uncompress it in batches
            std::array<segment_meta, details::FOR_buffer_depth>
              _uncompressed_head{};
            // contains the decoder where compressed data is read from
            r_state _active_frame{};
            // index into _uncompressed_head, or size() to mean empty
            uint8_t _head_ptr = _uncompressed_head.size();
            // index into _trailing_sm, see advance() to see why the starting
            // value is -1
            int8_t _trailing_ptr = {-1};
            // save how many segments are in _trailing_sm
            uint8_t _trailing_sz = {0};
            // array for trailing segment_ms. it's 1 less than the buffer
            std::array<segment_meta, details::FOR_buffer_depth - 1>
              _trailing_sm;
            // vector of w_states to read next after _active_frame is exhausted
            std::vector<ss::lw_shared_ptr<const w_state>> _frames_to_read;

            // construct a state from a range of frames, and performa an
            // optional skip operation also prime the pump
            explicit state(
              std::span<const segment_meta> trailing_buf,
              auto&& encoded_frames_range,
              hint_t const* maybe_hint = nullptr);

            // let's be explicit: clone or share, no implicit copy op allowed
            state(state const&) = delete;
            state& operator=(state const&) = delete;
            // move op are allowed
            state(state&& rhs) noexcept = default;
            state& operator=(state&&) noexcept = default;
            ~state() = default;

            // assert on the invariants of this state
            void check_valid() const;
            // clone performs a "deep" clone such as destructive op on this do
            // not modify the returned object
            auto clone() const -> state;
            // get current segment_meta (either from the head_buffer or from the
            // trailing buffer)
            auto get() const -> segment_meta const&;
            // we are interested to check if incrementing this would leads to
            // reaching end
            auto is_last_one() const noexcept -> bool;
            // performs ++
            void advance(hint_t const* hint = nullptr) noexcept;

        private:
            // this constructor is used only by clone
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

    // Since we're getting so many overloads from
    // sequence_container_interface, and since many of those overloads are
    // implemented in terms of a user-defined function of the same name, we
    // need to add quite a few using declarations here.
    using base_type
      = boost::stl_interfaces::sequence_container_interface<cstore_v2>;
    using base_type::begin;
    using base_type::end;

    cstore_v2() = default;
    cstore_v2(cstore_v2 const&);
    cstore_v2& operator=(cstore_v2 const&);
    cstore_v2(cstore_v2&&);
    cstore_v2& operator=(cstore_v2&&);

    auto begin() -> const_frame_iterator {
        return empty() ? end()
                       : const_frame_iterator{in_buffer_view(), _encoded_state};
    }
    auto end() -> const_frame_iterator { return {}; }
    void swap(cstore_v2& rhs) noexcept;
    size_t max_size() const;
    // these could be inherited by sequence_container but we can do better
    constexpr size_t size() const noexcept { return _size; }
    bool operator==(cstore_v2 const& rhs) const noexcept;

    // providing these offers some other possibilities
    cstore_v2(const_iterator const&, const_iterator const&);

    /// search methods

    // search by base offset
    auto find(model::offset) const noexcept -> const_frame_iterator;

    // data insertion operation
private:
    // private code path for appending segments to a tmp cstore while merging stuff. 
    void append(segment_meta const& sm) {
        {
        auto last_sr=get_last_segment_range();
        vassert(
          last_sr.base_offset < sm.base_offset && ,
          "append supports only tail insertion. current last_segment_range: {}, "
          "append candidate: {}",
          last_sr,
          sm);
        }
        if (in_buffer_size() < (_in_buffer.size() - 1)) {
            // there is space in the _in_buffer. just insert it there, since
            // there is no other observer of this data
            _in_buffer[in_buffer_size()] = sm;
            ++_size;
            return;
        }

        // there is enough data in the buffer + sm to insert a new batch in the
        // encoded state.
        if (unlikely(_encoded_state.empty())) {
            // ensure a least one
            _encoded_state.push_back(ss::make_lw_shared<w_state>());
        }

        if (unlikely(!_encoded_state.back().owned())) {
            // CoW, clone the last one since there is something holding onto it
            // and we are going to modify it
            _encoded_state.back()
        }
    }
};

/// it's like lower bound in the mathematical sense:
/// return: an iterator to the element equal to val or to the biggest element
/// __less__ than val. if no element is the mathematical lower bound, return
/// end. this is not strictly math-consistent but works ok in the context of
/// iterators
static auto find_val_or_less(auto&& rng, auto val, auto projection) {
    if (rng.empty()) {
        return rng.end();
    }
    // std::lower_bound does not return std::end(), so we can skip that check
    auto it = std::ranges::lower_bound(rng, val, std::less<>{}, projection);
    // exact match
    if (std::invoke(projection, *it) == val) {
        return it;
    }

    // no value can satisfy the search, since *it is greater than val here
    if (it == rng.begin()) {
        return rng.end();
    }

    // there is no element equal to val, but since *it is greater than val the
    // prev satisfy the search
    return std::prev(it);
}

auto cstore_v2::find(model::offset base_offset) const noexcept
  -> const_frame_iterator {
    // small optimization: ensure that base_offset might be in the range covered
    // by this
    if (
      base_offset < get_base_offset() || base_offset > get_committed_offset()) {
        return end();
    }

    // search a frame that might contain the base offset
    auto it = find_val_or_less(
      _encoded_state, base_offset, &w_state::get_base_offset);
    if (it == _encoded_state.end()) {
        // base offset could be in the in_buffer still
        auto tail_slice = in_buffer_view();
        auto buff_it = std::ranges::find(
          tail_slice, base_offset, &segment_meta::base_offset);
        if (buff_it == tail_slice.end()) {
            return end();
        }

        auto result_it = const_frame_iterator{
          tail_slice,
          std::ranges::subrange{_encoded_state.end(), _encoded_state.end()}};
        return std::next(result_it, std::distance(tail_slice.begin(), buff_it));
    }

    auto& frame_hints = (*it)->_frame_hints;
    auto hint_it = find_val_or_less(frame_hints, base_offset, &hint_t::key);
    auto hint_ptr = hint_it != frame_hints.end() ? &(*hint_it) : nullptr;

    // it points to a frame that might contain the base offset, start a
    // const_frame_iterator from it
    auto candidate = const_frame_iterator{
      in_buffer_view(),
      std::ranges::subrange{it, _encoded_state.end()},
      hint_ptr};
    // advance candidate
    for (; candidate->base_offset < base_offset; ++candidate) {
    }

    if (candidate->base_offset == base_offset) {
        return candidate;
    }

    return end();
}

void cstore_v2::swap(cstore_v2& rhs) noexcept {
    if (this == &rhs) {
        return;
    }
    auto us_in = in_buffer_view();
    auto them_in = rhs.in_buffer_view();
    auto us = std::tie(_encoded_state, us_in, _size);
    auto them = std::tie(rhs._encoded_state, them_in, rhs._size);
    std::swap(us, them);
}

bool cstore_v2::operator==(cstore_v2 const& rhs) const noexcept {
    // exclude hints, as they are just a lookup optimization. compare
    // fields in order of simplicity
    constexpr static auto dereference_encoded_state =
      [](auto& ptr) -> decltype(auto) { return *ptr; };
    return size() == rhs.size()
           && std::ranges::equal(in_buffer_view(), rhs.in_buffer_view())
           && std::ranges::equal(
             _encoded_state | std::views::transform(dereference_encoded_state),
             rhs._encoded_state
               | std::views::transform(dereference_encoded_state));
}
auto cstore_v2::get_base_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return {};
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
        return {};
    }

    if (_size <= _in_buffer.size()) {
        // no data in the encoders yet
        return _in_buffer[_size - 1].committed_offset;
    }

    return _encoded_state.back()->get_commited_offset();
}

template<typename Rng>
cstore_v2::const_frame_iterator::const_frame_iterator(
  std::span<const segment_meta> trailing_buf, Rng&& encoded_frames_range)
  : _state(ss::make_lw_shared<state>(
    trailing_buf, std::forward<Rng>(encoded_frames_range))) {}

template<typename Rng>
cstore_v2::const_frame_iterator::const_frame_iterator(
  std::span<const segment_meta> trailing_buf,
  Rng&& encoded_frames_range,
  hint_t const* maybe_hint)
  : _state(ss::make_lw_shared<state>(
    trailing_buf, std::forward<Rng>(encoded_frames_range), maybe_hint)) {}

auto cstore_v2::const_frame_iterator::operator*() const noexcept
  -> segment_meta const& {
    vassert(_state, "invariant: _state is valid if this is not end");
    return _state->get();
}
auto cstore_v2::const_frame_iterator::operator++() noexcept
  -> const_frame_iterator& {
    vassert(_state, "invariant: _state is valid if this is not end");
    if (_state->is_last_one()) {
        // advancing the state would land us in the end iterator, so
        // release the state to become an end iterator
        _state.release();
        return *this;
    }

    ensure_owned();
    _state->advance();
    return *this;
}

cstore_v2::const_frame_iterator::state::state(
  std::span<const segment_meta> trailing_buf,
  auto&& encoded_frames_range,
  hint_t const* maybe_hint)
  : _trailing_sz(trailing_buf.size())
  , _frames_to_read(
      std::ranges::begin(encoded_frames_range),
      std::ranges::end(encoded_frames_range)) {
    vassert(
      trailing_buf.size() < _trailing_sm.max_size(),
      "trailing_buf {} is too big (max: {})",
      trailing_buf.size(),
      _trailing_sm.max_size());
    std::ranges::copy(trailing_buf, _trailing_sm.begin());
    // prime the pump, this advance should be safe even in the various edge
    // cases
    advance(maybe_hint);
}

void cstore_v2::const_frame_iterator::state::check_valid() const {
    // this check works out also when we call the first advance in the
    // constructor to prime the pump
    vassert(
      (_head_ptr < _uncompressed_head.size() && _trailing_ptr == 0)
        || (_head_ptr == _uncompressed_head.size() && _trailing_ptr < _trailing_sz),
      "ensure invariants about the two buffers: that the either "
      "the head is being consumed or that the trailer is begin "
      "consumed (without being terminaned, since that would be "
      "like accessing end())");
}
auto cstore_v2::const_frame_iterator::state::clone() const -> state {
    check_valid(); // not strictly needed but it's useful to limit the
                   // number of possible states of the system
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

auto cstore_v2::const_frame_iterator::state::get() const
  -> segment_meta const& {
    check_valid();
    if (_head_ptr < _uncompressed_head.size()) {
        return _uncompressed_head[_head_ptr];
    }
    return _trailing_sm[_trailing_ptr];
}

auto cstore_v2::const_frame_iterator::state::is_last_one() const noexcept
  -> bool {
    check_valid();
    // if no more data in the buffers, then there should be only one
    // in the trailer
    if (
      _head_ptr == _uncompressed_head.size()
      && std::get<sm_base_offset_position>(_active_frame._frame_fields).empty()
      && _frames_to_read.empty()) {
        return _trailing_ptr == (_trailing_sz - 1);
    }

    // if there is data in the head buffer, it must be only one
    // datapoint and buffers need to be empty and no other data should
    // be in the trailer
    if (_head_ptr == _uncompressed_head.size() - 1) {
        return _trailing_sz == 0
               && std::get<sm_base_offset_position>(_active_frame._frame_fields)
                    .empty()
               && _frames_to_read.empty();
    }

    return false;
}

void cstore_v2::const_frame_iterator::state::advance(
  hint_t const* hint_p) noexcept {
    check_valid();

    _head_ptr = std::min<uint8_t>(_head_ptr + 1, _uncompressed_head.size());

    if (_head_ptr == _uncompressed_head.size()) {
        // reached the end of the head buffer, try to uncompress
        // further data
        if (_active_frame.empty()) {
            // _active_frame has no more data, create a new one from
            // the next _frame
            if (_frames_to_read.empty()) {
                // there are no more frames, we terminated all the
                // compressed data. time to access _trailing_sm since
                // _trailing_ptr starts at -1, the first increment
                // will bring it to 0 and we don't have to special
                // case it
                ++_trailing_ptr;
                return;
            }
            _active_frame = _frames_to_read.front()->get_r_state(hint_p);
            _frames_to_read.erase(_frames_to_read.begin());
            // ensure _active_frame has data to uncompress
        }
        // uncompress data
        // set back _head_ptr to the start of the buffer
        _head_ptr = 0;
        _active_frame.uncompress(_uncompressed_head);
    }
}

auto cstore_v2::r_state::empty() const noexcept -> bool {
    // any frame would be ok but since it's the main index, use
    // base_offset frame
    return std::get<sm_base_offset_position>(_frame_fields).empty();
}

auto cstore_v2::r_state::clone() const -> r_state {
    if (empty()) {
        // every empty r_state is the same
        return {};
    }
    // get a pristine r_state
    auto res = _parent->get_r_state();
    // fix up the _frame_fields by skipping ahead
    tuple_for<sm_num_fields>([&](auto ix) {
        // get a copy of the skip info
        auto original_pos
          = std::get<ix>(_frame_fields)
              .get_pos()
              .to_stream_pos_t(
                std::get<ix>(_parent->_frame_fields).get_position().offset);
        // apply them
        std::get<ix>(res._frame_fields).skip(original_pos);
    });

    return res;
}

void cstore_v2::r_state::uncompress(
  std::span<segment_meta, details::FOR_buffer_depth> out) {
    std::array<int64_t, details::FOR_buffer_depth> buffer;
    // uncompress each column into a intermediate buffer, save it
    // in out array
    tuple_for<sm_num_fields>([&](auto ix) {
        std::get<ix>(_frame_fields).read(buffer);

        for (auto i = 0u; i < buffer.size(); i++) {
            auto& field_is = std::get<ix>(reflection::to_tuple(out[i]));
            field_is = static_cast<std::decay_t<decltype(field_is)>>(buffer[i]);
        }
    });

    if (empty()) {
        // micro optimization: releases parent so that it can be free
        // to self modify
        _parent.release();
    }
}

auto cstore_v2::w_state::get_r_state(hint_t const* hint_p) const -> r_state {
    auto res = r_state{
      ._parent = shared_from_this(),
      ._frame_fields = tuple_for<sm_num_fields>([&](auto ix) {
          return decoder_t<ix>{
            std::get<ix>(_frame_fields).get_initial_value(),
            std::get<ix>(_frame_fields).get_row_count(),
            std::get<ix>(_frame_fields).share(),
            delta_alg_t<ix>{}};
      }),
    };

    if (hint_p != nullptr) {
        {
            auto frame_base_offset = std::get<sm_base_offset_position>(
                                       _frame_fields)
                                       .get_initial_value();
            auto frame_committed_offset
              = std::get<sm_committed_offset_position>(_frame_fields)
                  .get_last_value();
            vassert(
              hint_p->key >= frame_base_offset
                && hint_p->key <= frame_committed_offset,
              "hint base offset {} is outside of the range of this frame: [{} "
              "- {}]",
              hint_p->key(),
              frame_base_offset,
              frame_committed_offset);
        }
        // if we have a hint, apply it to each frame field
        tuple_for<sm_num_fields>(
          [&values = hint_p->mapped_value, &res](auto ix) {
              std::get<ix>(res._frame_fields).skip(values[ix]);
          });
    }
    return res;
};
} // namespace cloud_storage

BOOST_AUTO_TEST_SUITE(test_suite_cstore_v2);

BOOST_AUTO_TEST_CASE(hello_world) {}

BOOST_AUTO_TEST_SUITE_END();
