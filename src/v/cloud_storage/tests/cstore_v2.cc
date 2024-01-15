#include "cloud_storage/types.h"
#include "reflection/arity.h"
#include "serde/envelope.h"
#include "utils/delta_for.h"

#include <seastar/util/tuple_utils.hh>

#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <boost/test/unit_test.hpp>

#include <ranges>

namespace {

// call callable with all the [0, Sz) values and collect the results if any in a
// tuple
template<
  size_t Sz,
  std::invocable<std::integral_constant<std::size_t, 0>> Callable>
auto repeat_for(
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

template<typename tuple_t>
struct tuple_wrapper {
    tuple_t data{};

    constexpr explicit tuple_wrapper(tuple_t d) noexcept : data{std::move(d)}{}

    template<size_t Idx>
    constexpr auto operator[](std::integral_constant<size_t, Idx>) const
      -> decltype(auto) {
        return std::get<Idx>(data);
    }
    template<size_t Idx>
    constexpr auto operator[](std::integral_constant<size_t, Idx>)
      -> decltype(auto) {
        return std::get<Idx>(data);
    }

    constexpr auto operator==(tuple_wrapper const&) const -> bool = default;
};


} // namespace
namespace cloud_storage {

namespace details {
struct write_state;
struct const_iterator_state;
} // namespace details
struct cstore_v2
  : public boost::stl_interfaces::sequence_container_interface<cstore_v2> {
    // NOTEANDREA: static assert that segment_meta is not an envelope, we need
    // this to extract the correct tuple of fields (check)

    constexpr static auto max_frame_size = 1024u;
    constexpr static auto hints_sampling_rate = 8u;

    // number of fields in a segment meta
    constexpr static auto sm_num_fields = reflection::arity<segment_meta>();
    using sm_num_fields_t = std::integral_constant<std::size_t, sm_num_fields>;
    // which field is the base_offset. this gets a special treatment as it's the
    // key for this map
    constexpr static auto sm_base_offset_position
      = std::integral_constant<size_t, 2u>{};
    constexpr static auto sm_committed_offset_position
      = std::integral_constant<size_t, 3u>{};

    // projections from segment_meta to value_t used by the columns. the order
    // is the same of columns()
    constexpr static auto sm_readers = tuple_wrapper{std::tuple{
      [](segment_meta const& s) {
          return static_cast<int64_t>(s.is_compacted);
      },
      [](segment_meta const& s) { return static_cast<int64_t>(s.size_bytes); },
      [](segment_meta const& s) { return s.base_offset(); },
      [](segment_meta const& s) { return s.committed_offset(); },
      [](segment_meta const& s) { return s.base_timestamp(); },
      [](segment_meta const& s) { return s.max_timestamp(); },
      [](segment_meta const& s) { return s.delta_offset(); },
      [](segment_meta const& s) { return s.ntp_revision(); },
      [](segment_meta const& s) { return s.archiver_term(); },
      [](segment_meta const& s) { return s.segment_term(); },
      [](segment_meta const& s) { return s.delta_offset_end(); },
      [](segment_meta const& s) {
          return static_cast<std::underlying_type_t<segment_name_format>>(
            s.sname_format);
      },
      [](segment_meta const& s) {
          return static_cast<int64_t>(s.metadata_size_hint);
      },
    }};

    static_assert(
      reflection::arity<segment_meta>()
        == std::tuple_size_v<decltype(sm_readers{}.data)>,
      "segment_meta has a field that is not in segment_meta_accessors. check "
      "also that the members of column_store match the members of "
      "segment_meta");
    // delta_alg, encoder type and decoder type for each fields. they all get a
    // delta_xor compression, except the key field (base_offset) that gets a
    // delta_delta encoder, (it's monotonic)
    template<size_t idx>
    using delta_alg_t = std::conditional_t<
      idx == sm_base_offset_position,
      ::details::delta_delta<int64_t>,
      ::details::delta_xor>;
    template<size_t idx>
    using encoder_t = deltafor_encoder<int64_t, delta_alg_t<idx>, true>;
    template<size_t idx>
    using decoder_t = deltafor_decoder<int64_t, delta_alg_t<idx>>;

    // tuple-type with a deltafor_encoder for each field in a segment_meta.
    using encoder_tuple_t = tuple_wrapper<decltype(repeat_for<sm_num_fields>(
      [](auto ix) { return encoder_t<ix>{}; }))>;

    using encoder_in_buffer = std::array<int64_t, ::details::FOR_buffer_depth>;
    // for each key (the base offset of segment_meta), store the position of
    // each field in the corresponding deltafor_buffer

    // tuple-type with a deltafor_decoder for each field in a segment_meta.
    using decoder_tuple_t = tuple_wrapper<decltype(repeat_for<sm_num_fields>(
      [](auto ix) { return decoder_t<ix>{}; }))>;

    struct hint_t {
        model::offset key;
        std::array<deltafor_stream_pos_t<int64_t>, sm_num_fields> mapped_value;
    };

    struct segment_range {
        model::offset base_offset;
        model::offset committed_offset;
        segment_range() noexcept = default;
        segment_range(model::offset base, model::offset committed) noexcept
          : base_offset{base}
          , committed_offset{committed} {}
        explicit segment_range(segment_meta const& sm) noexcept
          : base_offset{sm.base_offset}
          , committed_offset{sm.committed_offset} {}

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
    // invariant: if _size is > details::FOR_buffer_depth, _encoded_state is not
    // empty and no w_state is ever empty
    std::vector<ss::lw_shared_ptr<details::write_state>> _encoded_state{};

    std::size_t _size{0};

    // returns base and committed offset covered by this span, as the base
    // offset of first element and commit offset of the last one
    auto get_base_offset() const noexcept -> model::offset;
    auto get_committed_offset() const noexcept -> model::offset;

public:
    auto get_range() const noexcept -> segment_range {
        return {get_base_offset(), get_committed_offset()};
    }

    auto get_last_segment_range() const noexcept -> segment_range;

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
        // create and iterator with data and an optional hint for the first
        // buffer (nullptr of emtpy)
        template<std::ranges::range Rng>
        explicit const_frame_iterator(
          Rng&& encoded_frames_range, hint_t const* maybe_hint = nullptr);

        // copy and move works as expected, sharing the lw_shared_ptr _state

        auto operator*() const noexcept -> segment_meta const&;
        auto operator++() noexcept -> const_frame_iterator&;

        friend bool operator==(
          const_frame_iterator const&, const_frame_iterator const&) noexcept;
        using base_type::operator++;

    private:
        // this is meant to be called from a non const method to ensure that a
        // destructive op does not touch other shared state
        void ensure_owned();

        // state is expensive, since this is a const_iterator it makes sense to
        // store it in a shared pointer with a copy_on_write mechanism
        // invariant: _state is valid if this is not end pointer
        ss::lw_shared_ptr<details::const_iterator_state> _state{};
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
    cstore_v2(cstore_v2&&) noexcept;
    cstore_v2& operator=(cstore_v2&&) noexcept;

    auto begin() -> const_frame_iterator {
        return empty() ? end() : const_frame_iterator{_encoded_state};
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
    // private code path for appending segments to a tmp cstore while merging
    // stuff.
    void push_back(segment_meta const& sm);
};

namespace details {

// utility iteration on each field

template<typename Callable>
constexpr auto for_fields(Callable&& functor) {
    return repeat_for<cstore_v2::sm_num_fields>(
      std::forward<Callable>(functor));
}

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

// struct used by const_iterator to read from a write_state.
// read_state points to the parent write_state, so that write_state knowns if
// there are readers, and to be able to clone itself when we need to iterate
// over a write_state, we create a read_state from it, call uncompress until
// empty. the struct is not smart, smartness is encoded in the handler
struct read_state {
    // parent is used to implement copy-on-write, since _frame_fields share
    // buffers with _parent
    ss::lw_shared_ptr<const write_state> _parent{};
    // the compressed data is read through these
    cstore_v2::decoder_tuple_t _frame_fields{};

    std::span<const segment_meta> _tail_buffer{};

    auto no_more_compressed_data() const noexcept -> bool {
        // any frame would be ok but since it's the main index, use
        // base_offset frame
        return _frame_fields[cstore_v2::sm_base_offset_position].empty();
    }
    // true: no more data can be uncompressed
    auto empty() const noexcept -> bool {
        // any frame would be ok but since it's the main index, use
        // base_offset frame
        return no_more_compressed_data() && _tail_buffer.empty();
    }
    // create a clone of r_state such that this->uncompress does not modify
    // the state of the new r_state
    auto clone() const -> read_state;
    // uncompress the next batch of segment_meta. modifies _frame_fields
    auto read_and_pop(std::span<segment_meta, ::details::FOR_buffer_depth> out)
      -> size_t;
};

struct write_state : public ss::enable_lw_shared_from_this<write_state> {
    // TODO add the ability to handle non modulo 16 number of segments:
    // while in normal append mode, the w_state is filled (and closed) in
    // blocks of 16, in replace mode the w_state should be able to replace
    // segments that spans to the next w_state. in general an aligned
    // replace operation creates a new w_state with a smaller size. how do
    // we handle this? an external buffer, or by padding the data? and if we
    // add an external buffer, do we get rid of the one in the cstore? in
    // the end, external buffer seems the easy-to-understand (and hopefully
    // to debug) solution

    // NOTEANDREA should be possible to have an upper bound and move this to
    // std::array invariants: this is sorted on key, has few elements, and
    // key in contained in the range of base_offsets covered by this span
    // this makes it so that on base_offset lookup we can linearly (or
    // binary) search this sequence for it (or the bigger value no bigger
    // than it) in the sequence, and it will give us an array of offsets
    // into the deltafor buffer, to speedup retrieval of encoded
    // segment_meta
    std::vector<cstore_v2::hint_t> _frame_hints{};

    // the deltafor encoders are contained here. invariants: all the
    // deltafor have the same size, together they encode all the
    // segment_meta saved in this frame
    cstore_v2::encoder_tuple_t _frame_fields{};

    // the segments_meta are kept here until they are enough (16) to be able
    // to inserted into _frame_fields a stronger type that allocates on the
    // heap but caps the size would be nice, but vector will do
    std::vector<segment_meta> _tail_buffer{};

    write_state() = default;
    // disable public copy and move since get_w_state generates back-links
    // to this object. use clone() and get_r_state() instead
private:
    struct lw_copy_t {};
    write_state(write_state const& lhs)
      : _frame_hints{lhs._frame_hints}
      , _frame_fields{for_fields([&](auto ix) {
          auto& lhs_enc = lhs._frame_fields[ix];
          return cstore_v2::encoder_t<ix>{
            lhs_enc.get_initial_value(),
            lhs_enc.get_row_count(),
            lhs_enc.get_last_value(),
            lhs_enc.copy()};
      })}
      , _tail_buffer{lhs._tail_buffer} {}
    write_state& operator=(write_state const& oth) {
        if (this != &oth) {
            auto cpy = oth;
            *this = std::move(cpy);
        }
        return *this;
    };
    write_state(write_state&&) = default;
    write_state& operator=(write_state&&) = default;
    write_state(write_state& lhs, lw_copy_t)
      : _frame_hints{lhs._frame_hints}
      , _frame_fields{for_fields([&](auto ix) {
          // this constructor shares the underlying buffer
          return cstore_v2::encoder_t<ix>(&(lhs._frame_fields[ix]));
      })}
      , _tail_buffer{lhs._tail_buffer} {}

public:
    ~write_state() = default;

    // used to implement copy-on-write: const_iterator grab a shared_ptr, if
    // some destructive operation (truncate, rewrite, append) has to happen
    // and has_reader() is true, a copy is created
    bool has_reader() const { return use_count() > 1; }

    friend bool
    operator==(write_state const& lhs, write_state const& rhs) noexcept {
        // only frame fields contains data that matters, _frame_hints are a
        // lookup optimization
        return std::tie(lhs._frame_fields, lhs._tail_buffer)
               == std::tie(rhs._frame_fields, rhs._tail_buffer);
    }

    auto get_base_offset() const -> model::offset {
        // precondition: this is not empty
        if (_frame_fields[cstore_v2::sm_base_offset_position].empty()) {
            return _tail_buffer[0].base_offset;
        }
        return model::offset{_frame_fields[cstore_v2::sm_base_offset_position]
                               .get_initial_value()};
    }

    auto get_commited_offset() const -> model::offset {
        // precondition: this is not empty
        return _tail_buffer.empty() ? model::offset(
                 _frame_fields[cstore_v2::sm_committed_offset_position]
                   .get_last_value())
                                    : _tail_buffer.back().committed_offset;
    }

    auto get_last_range() const noexcept -> cstore_v2::segment_range {
        // precondition: this is not empty;
        if (_tail_buffer.empty()) {
            return {
              model::offset(_frame_fields[cstore_v2::sm_base_offset_position]
                              .get_last_value()),
              model::offset(
                _frame_fields[cstore_v2::sm_committed_offset_position]
                  .get_last_value())};
        }
        return cstore_v2::segment_range{_tail_buffer.back()};
    }

    // create a read_state for this write_state. modifications to this could
    // reflect on write_state, that's why the resulting read_state has a
    // back-link to this, to implement copy-on-write optionally, if hint_p is
    // not nullptr, apply the hint to the r_state to skip ahead
    auto get_r_state(cstore_v2::hint_t const* hint_p = nullptr) const
      -> read_state {
        auto res = read_state{
          ._parent = shared_from_this(),
          ._frame_fields = {for_fields([&](auto ix) {
              return cstore_v2::decoder_t<ix>{
                _frame_fields[ix].get_initial_value(),
                _frame_fields[ix].get_row_count(),
                _frame_fields[ix].share(),
                cstore_v2::delta_alg_t<ix>{}};
          })},
          ._tail_buffer = _tail_buffer,
        };

        if (hint_p != nullptr) {
            // if we have a hint, apply it to each frame field
            for_fields([&](auto ix) {
                res._frame_fields[ix].skip(hint_p->mapped_value[ix]);
            });
        }
        return res;
    }

    // create a clone of this w_state, such that non-const op on the
    // returned object do not affect this
    auto clone() const -> ss::lw_shared_ptr<write_state> {
        // TODO check if it's possible to use lw_copy mechanism
        return ss::make_lw_shared<write_state>(*this);
    }

    auto size() const noexcept -> size_t {
        return _frame_fields[cstore_v2::sm_base_offset_position].get_row_count()
                 * ::details::FOR_buffer_depth
               + _tail_buffer.size();
    }

    void close_tail() {
        // assert owned();
        _tail_buffer.shrink_to_fit();
    }

    auto append(segment_meta const& sm) {
        // assert owned();
        _tail_buffer.push_back(sm);
        if(_tail_buffer.size()<::details::FOR_buffer_depth){
          return;
        }

        // enough data to encode
        for_fields([&](auto ix){
          std::array<int64_t, ::details::FOR_buffer_depth> buf;
          std::ranges::copy(_tail_buffer | std::views::transform([ix](auto& e){
            return cstore_v2::sm_readers[ix](std::get<ix>(reflection::to_tuple(e)));
          }), buf.begin());

          auto tx=_frame_fields[ix].tx_start();
          tx.add(buf);
          return tx;
        });
        // destructor of the tuple of tx above actually adds the data to our _frame fields.

        if((_frame_fields[cstore_v2::sm_base_offset_position].get_row_count() % cstore_v2::hints_sampling_rate) == 0){
          cstore_v2::hint_t hint;
            // TODO continue here
          for_fields([&](auto ix){
            hint.mapped_value[ix]=_frame_fields[ix].get_current_stream_pos();
          });

          hint.key=hint.mappe
          _frame_fields.
        }
    }
};

struct const_iterator_state {
    // array to contain data as we uncompress it in batches
    std::array<segment_meta, ::details::FOR_buffer_depth> _uncompressed_head{};
    // contains the decoder where compressed data is read from
    read_state _active_frame{};
    // index into _uncompressed_head, or size() to mean empty
    uint8_t _head_ptr = _uncompressed_head.size();

    // vector of w_states to read next after _active_frame is exhausted
    std::vector<ss::lw_shared_ptr<const write_state>> _frames_to_read;

    // construct a state from a range of frames, and perform an
    // optional skip operation also prime the pump
    explicit const_iterator_state(
      std::ranges::range auto&& encoded_frames_range,
      cstore_v2::hint_t const* maybe_hint)
      : _frames_to_read(
        std::ranges::begin(encoded_frames_range),
        std::ranges::end(encoded_frames_range)) {
        // prime the pump, this advance should be safe even in the various edge
        // cases
        prime_the_pump(maybe_hint);
    }

    // let's be explicit: clone or share, no implicit copy op allowed
    const_iterator_state(const_iterator_state const&) = delete;
    const_iterator_state& operator=(const_iterator_state const&) = delete;
    // move op are allowed
    const_iterator_state(const_iterator_state&& rhs) noexcept = default;
    const_iterator_state& operator=(const_iterator_state&&) noexcept = default;
    ~const_iterator_state() = default;

    // assert on the invariants of this state
    void check_valid() const {
        // this check works out also when we call the first advance in the
        // constructor to prime the pump
        vassert(_head_ptr <= _uncompressed_head.size(), "invariant broken");
        vassert(
          _active_frame._tail_buffer.size() < _uncompressed_head.size(),
          "tail buffer of current read_state must be able to fit in the "
          "internal buffer");
    }
    // clone performs a "deep" clone such as destructive op on this do
    // not modify the returned object
    auto clone() const -> const_iterator_state {
        check_valid(); // not strictly needed but it's useful to limit the
                       // number of possible states of the system
        if (empty()) {
            return {};
        }

        auto res = const_iterator_state{};

        res._head_ptr = _head_ptr;
        // just copy data that can be read
        std::ranges::copy(
          std::span{_uncompressed_head}.subspan(_head_ptr)
            | std::views::reverse,
          std::ranges::begin(res._uncompressed_head | std::views::reverse));

        // create a clone from the original cstore iobuf
        res._active_frame = _active_frame.clone();
        // copy the frames after this that will be read after the active
        // frame
        res._frames_to_read = _frames_to_read;
        return res;
    }
    // get current segment_meta (either from the head_buffer or from the
    // trailing buffer)
    auto get() const -> segment_meta const& {
        check_valid();
        vassert(
          _head_ptr < _uncompressed_head.size(), "attempted read out of range");
        return _uncompressed_head[_head_ptr];
    }
    auto empty() const noexcept -> bool {
        check_valid();
        return _head_ptr == _uncompressed_head.size() && _active_frame.empty()
               && _frames_to_read.empty();
    }
    // performs ++
    void advance() noexcept {
        check_valid();
        // invariant: at the end of this method _uncompressed_head + _head_ptr
        // will point to good data or this is an empty state
        _head_ptr = std::min<uint8_t>(_head_ptr + 1, _uncompressed_head.size());

        if (_head_ptr == _uncompressed_head.size()) {
            // reached the end of the head buffer, try to uncompress
            // further data
            if (_active_frame.empty()) {
                // _active_frame has no more data, create a new one from
                // the next _frame
                if (_frames_to_read.empty()) {
                    // there are no more frames, we terminated all the
                    // data. time to become an empty state
                    return;
                }
                _active_frame = _frames_to_read.front()->get_r_state();
                _frames_to_read.erase(_frames_to_read.begin());
                // ensure _active_frame has data to uncompress
            }
            _head_ptr = _active_frame.read_and_pop(_uncompressed_head);
        }
    }

private:
    // supposed to be called only in the constructor, when we want to jump
    // directly to a value pointed by hints
    void prime_the_pump(cstore_v2::hint_t const* hint_p) noexcept {
        // hint!= nullptr implies _frames_to_read.empty() == false
        vassert(
          (_frames_to_read.empty() && hint_p == nullptr)
            || !_frames_to_read.empty(),
          "hints can be applied only when there is data to read");
        if (_frames_to_read.empty()) {
            // edge case of an empty iterator
            return;
        }

        // delegate the hint application to get_r_state
        _active_frame = _frames_to_read.front()->get_r_state(hint_p);
        _frames_to_read.erase(_frames_to_read.begin());
        _head_ptr = _active_frame.read_and_pop(_uncompressed_head);
    }

    // this constructor is used only by clone
    const_iterator_state() = default;
};

auto read_state::clone() const -> read_state {
    if (empty()) {
        // every empty r_state is the same
        return {};
    }
    // get a pristine r_state
    auto res = _parent->get_r_state();
    // fix up the _frame_fields by skipping ahead
    for_fields([&](auto ix) {
        // get a copy of the skip info
        auto original_pos = _frame_fields[ix].get_pos().to_stream_pos_t(
          _parent->_frame_fields[ix].get_position().offset);
        // apply them
        res._frame_fields[ix].skip(original_pos);
    });

    // this op is safe because the parent is the same
    res._tail_buffer = _tail_buffer;

    return res;
}

auto read_state::read_and_pop(
  std::span<segment_meta, ::details::FOR_buffer_depth> out) -> size_t {
    if (no_more_compressed_data()) {
        // dump _tail_buffer at the end of out (this is for the benefit of the
        // caller) and return the new starting index for out. zeros _tail_buffer
        // so that read state now reads empty()
        std::ranges::copy(
          _tail_buffer | std::views::reverse,
          std::ranges::begin(out | std::views::reverse));
        auto out_first_valid_index = out.size() - _tail_buffer.size();
        _tail_buffer = {};
        _parent.release(); // micro optimization since now we know that
                           // this->empty() is true
        return out_first_valid_index;
    }

    // extract data from the compressed buffer
    std::array<int64_t, out.size()> buffer;
    // uncompress each column into a intermediate buffer, save it
    // in out array
    for_fields([&](auto ix) {
        _frame_fields[ix].read(buffer);
        using field_type = std::decay_t<
          std::tuple_element_t<ix, decltype(reflection::to_tuple(out[0]))>>;
        for (auto i = 0u; i < buffer.size(); i++) {
            std::get<ix>(reflection::to_tuple(out[i]))
              = static_cast<field_type>(buffer[i]);
        }
    });

    if (empty()) {
        // micro optimization: releases parent so that it can be free
        // to self modify
        _parent.release();
    }
    return 0;
}

} // namespace details

/// const_frame_iterator section

template<std::ranges::range Rng>
cstore_v2::const_frame_iterator::const_frame_iterator(
  Rng&& encoded_frames_range, hint_t const* maybe_hint)
  : _state(ss::make_lw_shared<details::const_iterator_state>(
    std::forward<Rng>(encoded_frames_range), maybe_hint)) {}

auto cstore_v2::const_frame_iterator::operator*() const noexcept
  -> segment_meta const& {
    vassert(_state, "invariant: _state is valid if this is not end");
    return _state->get();
}
auto cstore_v2::const_frame_iterator::operator++() noexcept
  -> const_frame_iterator& {
    vassert(_state, "invariant: _state is valid if this is not end");

    if (!_state.owned()) {
        // ++(*this) is a destructive op. if we are sharing a state, ensure that
        // there will be no other observer of the change
        _state = ss::make_lw_shared(_state->clone());
    }

    _state->advance();
    if (_state->empty()) {
        // state is empty, we are now the end iterator, so
        // release the state to maintain the invariant of end iterator having
        // null _state
        _state.release();
    }
    return *this;
}

bool operator==(
  cstore_v2::const_frame_iterator const& lhs,
  cstore_v2::const_frame_iterator const& rhs) noexcept {
    // get base_offset or default if end iterator
    auto lhs_base_off = lhs._state ? lhs._state->get().base_offset
                                   : model::offset();
    auto rhs_base_off = rhs._state ? rhs._state->get().base_offset
                                   : model::offset();
    return lhs_base_off == rhs_base_off;
}
/// cstore_v2 section
auto cstore_v2::get_last_segment_range() const noexcept -> segment_range {
    if (empty()) {
        return {};
    }

    return _encoded_state.back()->get_last_range();
}
void cstore_v2::swap(cstore_v2& rhs) noexcept {
    if (this == &rhs) {
        return;
    }
    auto us = std::tie(_encoded_state, _size);
    auto them = std::tie(rhs._encoded_state, rhs._size);
    std::swap(us, them);
}

bool cstore_v2::operator==(cstore_v2 const& rhs) const noexcept {
    return size() == rhs.size()
           && std::ranges::equal(
             _encoded_state, rhs._encoded_state, [](auto& l, auto& r) {
                 return *l == *r;
             });
}
auto cstore_v2::get_base_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return {};
    }
    // data in the encoders
    return _encoded_state.front()->get_base_offset();
}
auto cstore_v2::find(model::offset base_offset) const noexcept
  -> const_frame_iterator {
    // small optimization: ensure that base_offset might be in the range covered
    // by this
    if (
      base_offset < get_base_offset()
      || base_offset > get_last_segment_range().base_offset) {
        return end();
    }

    // search a frame that might contain the base offset
    auto it = details::find_val_or_less(
      _encoded_state, base_offset, &details::write_state::get_base_offset);
    if (it == _encoded_state.end()) {
        return end();
    }

    auto& frame_hints = (*it)->_frame_hints;
    auto hint_it = details::find_val_or_less(
      frame_hints, base_offset, &cstore_v2::hint_t::key);
    auto hint_ptr = hint_it != frame_hints.end() ? &(*hint_it) : nullptr;

    // it points to a frame that might contain the base offset, start a
    // const_frame_iterator from it
    auto candidate = const_frame_iterator{
      std::ranges::subrange{it, _encoded_state.end()}, hint_ptr};
    // advance candidate
    for (; candidate->base_offset < base_offset; ++candidate) {
    }

    if (candidate->base_offset == base_offset) {
        return candidate;
    }

    return end();
}

auto cstore_v2::get_committed_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return {};
    }
    return _encoded_state.back()->get_commited_offset();
}

void cstore_v2::push_back(segment_meta const& sm) {
    {
        auto last_sr = get_last_segment_range();
        vassert(
          last_sr.base_offset < sm.base_offset,
          "push_back supports only tail insertion. current "
          "last_segment_range: {}, "
          "push_back candidate: {}",
          last_sr,
          sm);

        vassert(
          last_sr.committed_offset <= sm.committed_offset,
          "candidate is inside the last segment last_segment_range: {} "
          "push_back candidate: {}",
          last_sr,
          sm);
    }

    if (
      _encoded_state.empty()
      || _encoded_state.back()->size() >= cstore_v2::max_frame_size) {
        // new cstore: add a new state or last frame reached max size: create a
        // new one,
        _encoded_state.push_back(ss::make_lw_shared<details::write_state>());
    }

    if (unlikely(!_encoded_state.back().owned())) {
        // CoW, clone the last one since there is something holding onto it
        // and we are going to modify it
        _encoded_state.back() = _encoded_state.back()->clone();
    }

    _encoded_state.back()->append(sm);
    ++_size;
}

} // namespace cloud_storage

BOOST_AUTO_TEST_SUITE(test_suite_cstore_v2);

BOOST_AUTO_TEST_CASE(hello_world) {}

BOOST_AUTO_TEST_SUITE_END();
