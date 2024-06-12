#include "json.h"
#include "json/ostreamwrapper.h"
#include "json/writer.h"

#include <ranges>

namespace {

constexpr auto additionalProperties = "additionalProperties";
constexpr auto type = "type";

enum class json_type {
    string = 0,
    integer,
    number,
    object,
    array,
    boolean,
    null
};

using pandaproxy::schema_registry::newer_schema;
using pandaproxy::schema_registry::older_schema;

constexpr auto json_schema_type_array
  = std::to_array<std::pair<json_type, std::string_view>>(
    {{json_type::string, "string"},
     {json_type::integer, "integer"},
     {json_type::number, "number"},
     {json_type::object, "object"},
     {json_type::array, "array"},
     {json_type::boolean, "boolean"},
     {json_type::null, "null"}});

// helper struct to format json::Value
struct printable_jsonv {
    json::Value const* v;
    friend std::ostream&
    operator<<(std::ostream& os, printable_jsonv const& p) {
        auto osw = json::OStreamWrapper{os};
        auto writer = json::Writer<json::OStreamWrapper>{osw};
        p.v->Accept(writer);
        return os;
    }
};

/// utility functions
constexpr std::optional<json_type>
schema_type_from_string(std::string_view sts) {
    auto t_it = std::ranges::find(
      json_schema_type_array, sts, [](auto t_s) { return t_s.second; });

    if (t_it == json_schema_type_array.end()) {
        return std::nullopt;
    } else {
        return t_it->first;
    }
}

constexpr std::string_view schema_type_to_string(json_type t) {
    return std::ranges::find(
             json_schema_type_array, t, [](auto t_s) { return t_s.first; })
      ->second;
}

std::istream& operator>>(std::istream& is, json_type& t) {
    auto sts = std::string{};
    is >> sts;
    auto opt_t = schema_type_from_string(sts);
    if (opt_t.has_value()) {
        t = opt_t.value();
    } else {
        is.setstate(std::ios::failbit);
    }
    return is;
}

// posit: like assert, but less harsh
template<typename... Args>
auto posit(
  bool check, fmt::format_string<Args...> msg_template, Args&&... args) {
    if (likely(check)) {
        return;
    }

    ss::throw_with_backtrace<
      pandaproxy::schema_registry::json_schema_exception>(
      fmt::format(msg_template, std::forward<Args...>(args...)));
}

std::set<json_type> normalized_type(json::Value const& v) {
    // parse None | schema_type | array[schema_type] into a std::set of types
    // TODO use a better datastructure as the return type
    auto type_it = v.FindMember(type);
    auto ret = std::set<json_type>{};
    if (type_it == v.MemberEnd()) {
        // omit keyword is like accepting all the types
        for (auto t : json_schema_type_array | std::views::keys) {
            ret.insert(t);
        }
    } else if (type_it->value.IsArray()) {
        for (auto const& type_val : type_it->value.GetArray()) {
            ret.insert(boost::lexical_cast<json_type>(std::string_view{
              type_val.GetString(), type_val.GetStringLength()}));
        }
    } else {
        ret.insert(boost::lexical_cast<json_type>(std::string_view{
          type_it->value.GetString(), type_it->value.GetStringLength()}));
    }
    return ret;
}

// true, false, or nullopt if it's a subschema to check recursively
std::pair<std::optional<bool>, json::Value const*>
normalized_additional_properties(json::Value const& v) {
    auto additional = v.FindMember(additionalProperties);
    if (additional == v.MemberEnd()) {
        // implicit additionalProperties: true
        return {true, nullptr};
    }

    if (additional->value.IsBool()) {
        // explicit additionalProperties: [boolean]
        return {additional->value.GetBool(), &additional->value};
    }
    // additionalProperties: [subschema]
    return {std::nullopt, &additional->value};
}

bool is_string_stricter(newer_schema newer, older_schema older) {
    posit(
      false,
      "{} not implemented. input: newer: '{}', older '{}'",
      __FUNCTION__,
      printable_jsonv{newer()},
      printable_jsonv{older()});
}
bool is_numeric_stricter(newer_schema newer, older_schema older) {
    posit(
      false,
      "{} not implemented. input: newer: '{}', older '{}'",
      __FUNCTION__,
      printable_jsonv{newer()},
      printable_jsonv{older()});
}
bool is_array_stricter(newer_schema newer, older_schema older) {
    posit(
      false,
      "{} not implemented. input: newer: '{}', older '{}'",
      __FUNCTION__,
      printable_jsonv{newer()},
      printable_jsonv{older()});
}

[[maybe_unused]] bool
is_additional_properties_stricter(newer_schema newer, older_schema older) {
    // encoding this truth table for `newer is a subschema of older`
    // | newer | older | n <: o |
    // |  ____ |  true |  true  |  most permissive older
    // | false |  ____ |  true  |  most strict newer
    // |~false | false | false  |  most strict older, newer less strict
    // |  true | ~true | false  |  most permissive newer, older is strict
    // |  ____ |  ____ | recurse|  both are subschema, need further checks

    auto [older_additional, older_val] = normalized_additional_properties(
      *older());
    if (older_additional == true) {
        // (maybe implicit) additionalProperties: true in older, no value in
        // newer can be less strict
        return true; // newer is subschema of older
    }

    auto [newer_additional, newer_val] = normalized_additional_properties(
      *newer());
    if (newer_additional == false) {
        // additionalProperties: false in newer, it's the most possible
        // strict policy
        return true; // newer is subschema of older
    }
    // newer is true or subschema

    if (older_additional == false) {
        // newer is less strict than older
        return false; // newer is not a subschema
    }
    // older is a subschema

    if (newer_additional == true) {
        // newer it most permissive, older is strict
        return false; // newer is not a subschema
    }
    // both older and newer are subschema
    return pandaproxy::schema_registry::is_stricter_or_equal(
      pandaproxy::schema_registry::newer_schema{newer_val},
      pandaproxy::schema_registry::older_schema{older_val});
}
bool is_object_stricter(newer_schema newer, older_schema older) {
    posit(
      false,
      "{} not implemented. input: newer: '{}', older '{}'",
      __FUNCTION__,
      printable_jsonv{newer()},
      printable_jsonv{older()});
}

} // namespace

bool pandaproxy::schema_registry::is_stricter_or_equal(
  newer_schema newer, older_schema older) {
    // Checks that newer is stricter or equal than older, where stricter is
    // defined as "every json document valid by newer is also valid by older".
    // The precondition is that the schemas are valid json schemas draft4.
    // The test is done by searching a rule in newer that is less strict than
    // the equivalent in older, and returning true otherwise

    // extract { "type" : ... }
    auto older_types = normalized_type(*older());
    auto newer_types = normalized_type(*newer());

    // looking for types that are new in `newer`. done as newer_types
    // \ older_types
    auto newer_minus_older = std::set<json_type>{};
    std::ranges::set_difference(
      newer_types,
      older_types,
      std::inserter(newer_minus_older, newer_minus_older.end()));
    if (
      !newer_minus_older.empty()
      && !(
        newer_minus_older == std::set{json_type::integer}
        && older_types.contains(json_type::number))) {
        // newer_types_not_in_older accepts integer, and we can accept an
        // evolution from number -> integer. everything else is makes `newer`
        // less strict than older
        return false;
    }

    // newer accepts less (or equal) types. for each type, try to find a less
    // strict check
    for (auto t : newer_types) {
        // TODO this will perform a depth first search, but it might be better
        // to do a breadth first search to find a counterexample
        switch (t) {
        case json_type::string:
            if (!is_string_stricter(newer, older)) {
                return false;
            }
            break;
        case json_type::integer:
            [[fallthrough]];
        case json_type::number:
            if (!is_numeric_stricter(newer, older)) {
                return false;
            }
            break;
        case json_type::object:
            if (!is_object_stricter(newer, older)) {
                return false;
            }
            break;
        case json_type::array:
            if (!is_array_stricter(newer, older)) {
                return false;
            }
            break;
        case json_type::boolean:
            // no check needed for boolean;
            break;
        case json_type::null:
            // no check needed for null;
            break;
        }
    }

    return true;
}
