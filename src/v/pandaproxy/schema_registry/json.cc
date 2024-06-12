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

#include "pandaproxy/schema_registry/json.h"

#include "json/allocator.h"
#include "json/document.h"
#include "json/encodings.h"
#include "json/istreamwrapper.h"
#include "json/ostreamwrapper.h"
#include "json/schema.h"
#include "json/stringbuffer.h"
#include "json/types.h"
#include "json/writer.h"
#include "pandaproxy/schema_registry/error.h"
#include "pandaproxy/schema_registry/errors.h"
#include "pandaproxy/schema_registry/sharded_store.h"
#include "pandaproxy/schema_registry/types.h"
#include "strings/string_switch.h"

#include <seastar/core/coroutine.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/coroutine/exception.hh>
#include <seastar/util/defer.hh>

#include <absl/container/flat_hash_set.h>
#include <boost/outcome/std_result.hpp>
#include <boost/outcome/success_failure.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <rapidjson/error/en.h>

#include <exception>
#include <memory>
#include <stack>
#include <string_view>

namespace pandaproxy::schema_registry {

namespace {

// from https://json-schema.org/draft-04/schema, this is used to meta-validate a
// jsonschema
constexpr std::string_view json_draft_4_metaschema = R"json(
{
    "id": "http://json-schema.org/draft-04/schema#",
    "$schema": "http://json-schema.org/draft-04/schema#",
    "description": "Core schema meta-schema",
    "definitions": {
        "schemaArray": {
            "type": "array",
            "minItems": 1,
            "items": { "$ref": "#" }
        },
        "positiveInteger": {
            "type": "integer",
            "minimum": 0
        },
        "positiveIntegerDefault0": {
            "allOf": [ { "$ref": "#/definitions/positiveInteger" }, { "default": 0 } ]
        },
        "simpleTypes": {
            "enum": [ "array", "boolean", "integer", "null", "number", "object", "string" ]
        },
        "stringArray": {
            "type": "array",
            "items": { "type": "string" },
            "minItems": 1,
            "uniqueItems": true
        }
    },
    "type": "object",
    "properties": {
        "id": {
            "type": "string"
        },
        "$schema": {
            "type": "string"
        },
        "title": {
            "type": "string"
        },
        "description": {
            "type": "string"
        },
        "default": {},
        "multipleOf": {
            "type": "number",
            "minimum": 0,
            "exclusiveMinimum": true
        },
        "maximum": {
            "type": "number"
        },
        "exclusiveMaximum": {
            "type": "boolean",
            "default": false
        },
        "minimum": {
            "type": "number"
        },
        "exclusiveMinimum": {
            "type": "boolean",
            "default": false
        },
        "maxLength": { "$ref": "#/definitions/positiveInteger" },
        "minLength": { "$ref": "#/definitions/positiveIntegerDefault0" },
        "pattern": {
            "type": "string",
            "format": "regex"
        },
        "additionalItems": {
            "anyOf": [
                { "type": "boolean" },
                { "$ref": "#" }
            ],
            "default": {}
        },
        "items": {
            "anyOf": [
                { "$ref": "#" },
                { "$ref": "#/definitions/schemaArray" }
            ],
            "default": {}
        },
        "maxItems": { "$ref": "#/definitions/positiveInteger" },
        "minItems": { "$ref": "#/definitions/positiveIntegerDefault0" },
        "uniqueItems": {
            "type": "boolean",
            "default": false
        },
        "maxProperties": { "$ref": "#/definitions/positiveInteger" },
        "minProperties": { "$ref": "#/definitions/positiveIntegerDefault0" },
        "required": { "$ref": "#/definitions/stringArray" },
        "additionalProperties": {
            "anyOf": [
                { "type": "boolean" },
                { "$ref": "#" }
            ],
            "default": {}
        },
        "definitions": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "properties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "patternProperties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "dependencies": {
            "type": "object",
            "additionalProperties": {
                "anyOf": [
                    { "$ref": "#" },
                    { "$ref": "#/definitions/stringArray" }
                ]
            }
        },
        "enum": {
            "type": "array",
            "minItems": 1,
            "uniqueItems": true
        },
        "type": {
            "anyOf": [
                { "$ref": "#/definitions/simpleTypes" },
                {
                    "type": "array",
                    "items": { "$ref": "#/definitions/simpleTypes" },
                    "minItems": 1,
                    "uniqueItems": true
                }
            ]
        },
        "format": { "type": "string" },
        "allOf": { "$ref": "#/definitions/schemaArray" },
        "anyOf": { "$ref": "#/definitions/schemaArray" },
        "oneOf": { "$ref": "#/definitions/schemaArray" },
        "not": { "$ref": "#" }
    },
    "dependencies": {
        "exclusiveMaximum": [ "maximum" ],
        "exclusiveMinimum": [ "minimum" ]
    },
    "default": {}
}
)json";

class remote_schema_document_provider final
  : public rapidjson::IGenericRemoteSchemaDocumentProvider<
      json::SchemaDocument> {
public:
    const json::SchemaDocument* GetRemoteDocument(
      [[maybe_unused]] const char* uri,
      [[maybe_unused]] rapidjson::SizeType length) override {
        // Resolve the uri and returns a pointer to that schema.
        return nullptr;
    }
};

} // namespace

struct json_schema_definition::impl {
    ss::sstring to_json() const {
        json::StringBuffer buf;
        json::Writer<json::StringBuffer> wrt(buf);
        doc.Accept(wrt);
        return {buf.GetString(), buf.GetLength()};
    }

    explicit impl(
      json::Document doc,
      std::string_view name,
      std::unique_ptr<remote_schema_document_provider> rsdp)
      : doc{std::move(doc)}
      , rsdp{std::move(rsdp)}
      , schema(doc, name.data(), name.size(), rsdp.get()) {}

    json::Document doc;
    std::unique_ptr<remote_schema_document_provider> rsdp;
    json::SchemaDocument schema;
};

bool operator==(
  const json_schema_definition& lhs, const json_schema_definition& rhs) {
    return lhs.raw() == rhs.raw();
}

std::ostream& operator<<(std::ostream& os, const json_schema_definition& def) {
    fmt::print(
      os,
      "type: {}, definition: {}",
      to_string_view(def.type()),
      def().to_json());
    return os;
}

canonical_schema_definition::raw_string json_schema_definition::raw() const {
    return canonical_schema_definition::raw_string{_impl->to_json()};
}

ss::sstring json_schema_definition::name() const {
    return {
      _impl->schema.GetURI().GetString(),
      _impl->schema.GetURI().GetStringLength()};
};

ss::future<json_schema_definition>
make_json_schema_definition(sharded_store&, canonical_schema schema) {
    std::optional<std::exception> ex;
    try {
        json::Document doc;

        if (!doc.Parse(schema.def().raw()()).HasParseError()) {
            // fail;
        }
        // Populate impl->rsdp from doc and schema.def().refs()
        auto rsdp = std::make_unique<remote_schema_document_provider>();

        auto impl = ss::make_shared<json_schema_definition::impl>(
          std::move(doc), schema.sub()(), std::move(rsdp));

        co_return json_schema_definition{std::move(impl), schema.def().refs()};
    } catch (const std::exception& e) {
        ex = e;
    }
    co_return ss::coroutine::exception(
      std::make_exception_ptr(as_exception(error_info{
        error_code::schema_invalid,
        fmt::format("Invalid schema {}", ex->what())})));
}

ss::future<canonical_schema>
make_canonical_json_schema(sharded_store&, unparsed_schema def) {
    // TODO, pretty print

    // validation pre-step: compile metaschema for json draft
    static const auto metaschema_doc = [] {
        auto metaschema_json = json::Document{};
        metaschema_json.Parse(
          json_draft_4_metaschema.data(), json_draft_4_metaschema.size());
        vassert(
          !metaschema_json.HasParseError(), "Malformed metaschema document");

        return json::SchemaDocument{metaschema_json};
    }();

    // validation of schema: validate it against metaschema
    // first construct a reader that validates the schema against the metaschema
    // while parsing it
    auto schema_stream = std::istringstream{def.def().raw()()};
    auto stream_wrapper = json::IStreamWrapper{schema_stream};
    auto validating_reader = json::SchemaValidatingReader<json::IStreamWrapper>{
      stream_wrapper, metaschema_doc};

    // then parse schema to json
    auto schema_json = json::Document{};
    schema_json.Populate(validating_reader);

    if (auto parse_res = validating_reader.GetParseResult();
        parse_res.IsError()) {
        // schema_json is either not a json document
        // or it's not a valid json according to metaschema

        // Check the validation result
        if (!validating_reader.IsValid()) {
            // not a valid schema draft4 according to metaschema. retrieve some
            // info and throw
            auto error_loc_metaschema = json::StringBuffer{};
            auto error_loc_schema = json::StringBuffer{};
            validating_reader.GetInvalidSchemaPointer().StringifyUriFragment(
              error_loc_metaschema);
            validating_reader.GetInvalidDocumentPointer().StringifyUriFragment(
              error_loc_schema);
            auto invalid_keyword = validating_reader.GetInvalidSchemaKeyword();

            throw exception{
              error_code::schema_invalid,
              fmt::format(
                "Invalid json schema: '{}', invalid metaschema: '{}', invalid "
                "keyword: '{}'",
                std::string_view{
                  error_loc_schema.GetString(), error_loc_schema.GetLength()},
                std::string_view{
                  error_loc_metaschema.GetString(),
                  error_loc_metaschema.GetLength()},
                invalid_keyword)};
        } else {
            // not a valid json document
            throw exception{
              error_code::schema_invalid,
              fmt::format(
                "Malformed json schema: '{}' @{}",
                rapidjson::GetParseError_En(parse_res.Code()),
                parse_res.Offset())};
        }
    }

    // schema_json is a valid json and a syntactically valid json schema draft4.
    // TODO cross validate '#ref' tags, this is not done automatically

    co_return canonical_schema{
      def.sub(), canonical_schema_definition{def.def().raw(), def.type()}};
}

bool check_compatible(
  const json_schema_definition& reader, const json_schema_definition& writer) {
    return is_stricter_or_equal(
      newer_schema{&writer().doc}, older_schema{&reader().doc});
}

} // namespace pandaproxy::schema_registry
