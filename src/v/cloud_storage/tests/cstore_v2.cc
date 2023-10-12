#include "cloud_storage/types.h"
#include "reflection/arity.h"
#include "serde/envelope.h"
#include "utils/delta_for.h"

#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <boost/test/unit_test.hpp>

namespace cloud_storage {

struct cstore_frame
  : public boost::stl_interfaces::sequence_container_interface<cstore_frame> {
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
    // NOTEANDREA should be possible to have an upper bound and move this to
    // std::array invariants: this is sorted on key, has few elements, and key
    // in contained in the range of base_offsets covered by this span this makes
    // it so that on base_offset lookup we can linearly (or binary) search this
    // sequence for it (or the bigger value no bigger than it) in the sequence,
    // and it will give us an array of offsets into the deltafor buffer, to
    // speedup retrieval of encoded segment_meta
    std::vector<hint_t> _frame_hints{};

    // the deltafor encoders are contained here. invariants: all the deltafor
    // have the same size, together they encode all the segment_meta saved in
    // this frame
    encoder_tuple_t _frame_fields{};

    // before they can encoded in the encoders, we need to accumulate
    // #details::FOR_buffer_depth segment_meta objects. this in_buffer contains
    // 1 less then that. when there is a request to append the last one, they
    // will all be columnar-projected and encoded in the encoders, so this
    // buffer will never fill to full capacity
    std::array<segment_meta, details::FOR_buffer_depth - 1> _in_buffer{};

    // size doubles down as an index in _in_buffer, since _frame_fields contains
    // N*details::FOR_buffer_depth elements
    std::size_t _size{0};

    // helper to construct a reader
    static auto to_decoder_tuple(encoder_tuple_t& in) noexcept
      -> decoder_tuple_t;

public:
    // returns base and committed offset covered by this span, as the base
    // offset of first element and commit offset of the last one
    auto get_base_offset() const noexcept -> model::offset;
    auto get_committed_offset() const noexcept -> model::offset;

    struct const_frame_iterator
      : public boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta> {
        using base_type = boost::stl_interfaces::proxy_iterator_interface<
          const_frame_iterator,
          std::forward_iterator_tag,
          segment_meta>;
        constexpr const_frame_iterator() = default;
        constexpr explicit const_frame_iterator(
          cstore_frame const& cf) noexcept;
        constexpr segment_meta& operator*() const noexcept;
        constexpr const_frame_iterator& operator++() noexcept;
        friend constexpr bool operator==(
          const_frame_iterator const& lhs,
          const_frame_iterator const& rhs) noexcept {
            return lhs._pseudo_base_offset == rhs._pseudo_base_offset;
        }
        using base_type::operator++;

    private:
        // if this iterator is not end, this value is the actual base_offset of
        // the pointed segment_meta if this iterator is end, this value is the
        // sentinel value model::offset, and this an invariant of the iterator:
        // if the data pointed to is exhausted, this value is set to
        // model::offset
        model::offset _pseudo_base_offset{};
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

    cstore_frame() = default;
    cstore_frame(cstore_frame const&);
    cstore_frame& operator=(cstore_frame const&);
    cstore_frame(cstore_frame&&);
    cstore_frame& operator=(cstore_frame&&);

    inline const_frame_iterator begin() { return const_frame_iterator{*this}; }
    inline const_frame_iterator end() { return {}; }
    void swap(cstore_frame& rhs) noexcept;
    size_t max_size() const;
    // these could be inherited by sequence_container but we can do better
    constexpr size_t size() const noexcept { return _size; }
    bool operator==(cstore_frame const& rhs) const noexcept;

    // providing these offers some other possibilities
    cstore_frame(const_iterator const&, const_iterator const&);
};

void cstore_frame::swap(cstore_frame& rhs) noexcept {
    if (this == &rhs) {
        return;
    }
    auto us = std::tie(_frame_hints, _frame_fields, _in_buffer, _size);
    auto them = std::tie(
      rhs._frame_hints, rhs._frame_fields, rhs._in_buffer, rhs._size);
    std::swap(us, them);
}

bool cstore_frame::operator==(cstore_frame const& rhs) const noexcept {
    // exclude hints, as they are just a lookup optimization. compare fields in
    // order of simplicity
    return std::tie(_size, _in_buffer, _frame_fields)
           == std::tie(rhs._size, rhs._in_buffer, rhs._frame_fields);
}

auto cstore_frame::to_decoder_tuple(encoder_tuple_t& in) noexcept
  -> decoder_tuple_t {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
        return decoder_tuple_t{decoder_t<Is>{
          std::get<Is>(in).get_initial_value(),
          std::get<Is>(in).get_row_count(),
          std::get<Is>(in).share(),
          delta_alg_t<Is>{}}...};
    }(std::make_index_sequence<std::tuple_size_v<encoder_tuple_t>>());
}

auto cstore_frame::get_base_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return model::offset{};
    }
    if (_size <= _in_buffer.size()) {
        // no data in the encoders yet
        return _in_buffer[0].base_offset;
    }

    // data in the encoders
    return model::offset(
      std::get<sm_base_offset_position>(_frame_fields).get_initial_value());
}
auto cstore_frame::get_committed_offset() const noexcept -> model::offset {
    if (_size == 0) {
        return model::offset{};
    }

    if (_size <= _in_buffer.size()) {
        // no data in the encoders yet
        return _in_buffer[_size - 1].committed_offset;
    }

    return model::offset(
      std::get<sm_committed_offset_position>(_frame_fields).get_last_value());
}
constexpr cstore_frame::const_frame_iterator::const_frame_iterator(
  cstore_frame const& cf)
  : _pseudo_base_offset(cf.get_base_offset()) {}

} // namespace cloud_storage

BOOST_AUTO_TEST_SUITE(test_suite_cstore_v2);

BOOST_AUTO_TEST_CASE(hello_world) {}

BOOST_AUTO_TEST_SUITE_END();
