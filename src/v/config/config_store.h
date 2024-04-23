/*
 * Copyright 2020 Redpanda Data, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once

#include "base/seastarx.h"
#include "config/property.h"
#include "utils/to_string.h"

#include <fmt/format.h>

#include <unordered_map>

namespace config {
class config_store {
public:
    bool contains(std::string_view name);

    base_property& get(const std::string_view& name);

    using error_map_t = std::map<ss::sstring, ss::sstring>;

    /**
     * Missing or invalid properties whose metadata specifies `required=true`
     * are fatal errors, raised as std::invalid_argument.
     *
     * Other validation errors on property values are returned in a map
     * of property name to error message.  This includes malformed YAML,
     * bad YAML type, or an error flagged by the property's validator hook.
     *
     * @param root_node
     * @param ignore_missing Tolerate extra values in the config if they are
     *        contained in this set -- this is for reading old configs that
     *        mix node & cluster config properties.
     * @return map of property name to error.  Empty on clean load.
     */
    virtual error_map_t read_yaml(
      const YAML::Node& root_node,
      const std::set<std::string_view> ignore_missing = {});

    template<typename Func>
    void for_each(Func&& f) const {
        for (auto const& [_, property] : _properties) {
            f(*property);
        }
    }

    /**
     *
     * @param filter optional callback for filtering out config properties.
     *               callback should return false to exclude a property.
     */
    void to_json(
      json::Writer<json::StringBuffer>& w,
      redact_secrets redact,
      std::optional<std::function<bool(base_property&)>> filter
      = std::nullopt) const;

    void to_json_for_metrics(json::Writer<json::StringBuffer>& w);

    std::set<std::string_view> property_names() const;

    std::set<std::string_view> property_aliases() const;

    std::set<std::string_view> property_names_and_aliases() const;

    friend std::ostream&
    operator<<(std::ostream& o, const config::config_store& c);

    void notify_original_version(legacy_version ov);
    virtual ~config_store() noexcept = default;

private:
    friend class base_property;
    std::unordered_map<std::string_view, base_property*> _properties;

    // If a property has some aliases for backward compat, they are tracked
    // here: a property must appear at least in _properties, and may appear
    // 0..n times in _aliases
    std::unordered_map<std::string_view, base_property*> _aliases;
};

YAML::Node to_yaml(const config_store& cfg, redact_secrets redact);
}; // namespace config
