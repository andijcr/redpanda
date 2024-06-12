/*
 * Copyright 2024 Redpanda Data, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once

#include "json/document.h"
#include "pandaproxy/schema_registry/errors.h"
#include "pandaproxy/schema_registry/fwd.h"
#include "pandaproxy/schema_registry/types.h"

namespace pandaproxy::schema_registry {

ss::future<json_schema_definition>
make_json_schema_definition(sharded_store& store, canonical_schema schema);

ss::future<canonical_schema>
make_canonical_json_schema(sharded_store& store, unparsed_schema def);

bool check_compatible(
  const json_schema_definition& reader, const json_schema_definition& writer);

using newer_schema = named_type<json::Value const*, struct newer_schema_tag>;
using older_schema = named_type<json::Value const*, struct older_schema_tag>;
class json_schema_exception : public std::logic_error {};
bool is_stricter_or_equal(newer_schema newer, older_schema older);

ss::future<bool> check_compatible_in_the_weird_way(
  json_schema_definition reader, json_schema_definition writer);

} // namespace pandaproxy::schema_registry
