// Copyright 2024 Redpanda Data, Inc.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.md
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0

#include "pandaproxy/schema_registry/json.h"
#include "pandaproxy/schema_registry/sharded_store.h"
#include "pandaproxy/schema_registry/types.h"

#include <seastar/testing/thread_test_case.hh>
#include <seastar/util/defer.hh>

namespace {

namespace pp = pandaproxy;
namespace pps = pp::schema_registry;

constexpr std::string_view schema_1 = R"({
  "type": "object",
  "properties": {
    "foo": { "type": "string" },
    "bar": { "type": "string" }
  },
  "additionalProperties": true
})";

constexpr std::string_view schema_2 = R"({
  "type": "object",
  "properties": {
    "foo": { "type": "string" },
    "bar": { "type": "string" }
  },
  "additionalProperties": false
})";

bool check_compatible(
  const pps::canonical_schema_definition& r,
  const pps::canonical_schema_definition& w) {
    pps::sharded_store s;
    return pandaproxy::schema_registry::check_compatible(
      pps::make_json_schema_definition(
        s, {pps::subject("r"), {r.raw(), pps::schema_type::json}})
        .get(),
      pps::make_json_schema_definition(
        s, {pps::subject("w"), {w.raw(), pps::schema_type::json}})
        .get());
}
} // namespace

SEASTAR_THREAD_TEST_CASE(test_json_parse) {
    pps::canonical_schema_definition s1{schema_1, pps::schema_type::json};
    pps::canonical_schema_definition s2{schema_2, pps::schema_type::json};
    // reader is the new one
    BOOST_REQUIRE(check_compatible(s1, s2));
}


SEASTAR_THREAD_TEST_CASE(test_schema_parse){
  auto store=pps::sharded_store();
  store.start(pps::is_mutable::yes, ss::default_smp_service_group()).get();
  auto stop_store = ss::defer([&store]() { store.stop().get(); });

  
  pps::make_canonical_json_schema(sharded_store &store, unparsed_schema def)  

}
