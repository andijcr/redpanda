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

    // encoder type for each fields. they all get a delta_xor compression,
    // except the key field (base_offset) that gets a delta_delta encoder, (it's
    // monotonic)
    template<size_t idx>
    using encoder_t = deltafor_encoder<
      int64_t,
      std::conditional_t<
        idx == sm_base_offset_position,
        details::delta_delta<int64_t>,
        details::delta_xor>,
      true>;
    // tuple-type with a deltafor_encoder for each field in a segment_meta.
    using tuple_t = decltype([]<size_t... Is>(std::index_sequence<Is...>) {
        return std::tuple{encoder_t<Is>{}...};
    }(std::make_index_sequence<sm_num_fields>()));

    // for each key (the base offset of segment_meta), store the position of
    // each field in the corresponding deltafor_buffer
    struct hint_t {
        model::offset key;
        std::array<size_t, sm_num_fields> mapped_value;
    };

    // NOTEANDREA should be possible to have an upper bound and move this to
    // std::array invariants: this is sorted on key, has few elements, and key
    // in contained in the range of base_offsets covered by this span this makes
    // it so that on base_offset lookup we can linearly (or binary) search this
    // sequence for it (or the bigger value no bigger than it) in the sequence,
    // and it will give us an array of offsets into the deltafor buffer, to
    // speedup retrieval of encoded segment_meta
    std::vector<hint_t> frame_hints;

    // the deltafor encoders are contained here. invariants: all the deltafor
    // have the same size, together they encode all the segment_meta saved in
    // this frame
    tuple_t frame_fields;

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
        constexpr explicit const_frame_iterator(cstore_frame const& cf);
        constexpr segment_meta& operator*() const noexcept;
        constexpr const_frame_iterator& operator++() noexcept;
        friend constexpr bool operator==(
          const_frame_iterator const& lhs,
          const_frame_iterator const& rhs) noexcept;
        using base_type::operator++;
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

    const_frame_iterator begin();
    const_frame_iterator end();
    bool swap(cstore_frame&);
    size_t max_size() const;
    // these could be inherited by sequence_container but we can do better
    bool size() const;
    bool operator==(cstore_frame const&) const;

    // providing these offers some other possibilities
    cstore_frame(const_iterator const&, const_iterator const&);
};

} // namespace cloud_storage

BOOST_AUTO_TEST_SUITE(test_suite_cstore_v2);

BOOST_AUTO_TEST_CASE(hello_world) {}

BOOST_AUTO_TEST_SUITE_END();
