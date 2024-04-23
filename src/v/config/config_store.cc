#include "config_store.h"

namespace config {
bool config_store::contains(std::string_view name) {
    return _properties.contains(name) || _aliases.contains(name);
}

base_property& config_store::get(const std::string_view& name) {
    if (auto found = _properties.find(name); found != _properties.end()) {
        return *(found->second);
    } else if (auto found = _aliases.find(name); found != _aliases.end()) {
        found->second->aliases()[0].start_depr < return *(found->second);
    } else {
        throw std::out_of_range(fmt::format("Property {} not found", name));
    }
}

config_store::error_map_t config_store::read_yaml(
  const YAML::Node& root_node,
  const std::set<std::string_view> ignore_missing) {
    error_map_t errors;

    for (auto const& [name, property] : _properties) {
        if (property->is_required() == required::no) {
            continue;
        }
        ss::sstring name_str(name.data());
        if (!root_node[name_str]) {
            throw std::invalid_argument(
              fmt::format("Property {} is required", name));
        }
    }

    for (auto const& node : root_node) {
        auto name = node.first.as<ss::sstring>();
        auto* prop = [&]() -> base_property* {
            auto found = _properties.find(name);
            if (found != _properties.end()) {
                return found->second;
            }
            found = _aliases.find(name);
            if (found != _aliases.end()) {
                return found->second;
            }

            return nullptr;
        }();

        if (prop == nullptr) {
            if (!ignore_missing.contains(name)) {
                throw std::invalid_argument(
                  fmt::format("Unknown property {}", name));
            }
            continue;
        }
        bool ok = false;
        try {
            auto validation_err = prop->validate(node.second);
            if (validation_err.has_value()) {
                errors[name] = fmt::format(
                  "Validation error: {}",
                  validation_err.value().error_message());
            }
            prop->set_value(node.second);
            ok = true;
        } catch (YAML::InvalidNode const& e) {
            errors[name] = fmt::format("Invalid syntax: {}", e);
        } catch (YAML::ParserException const& e) {
            errors[name] = fmt::format("Invalid syntax: {}", e);
        } catch (YAML::BadConversion const& e) {
            errors[name] = fmt::format("Invalid value: {}", e);
        }

        // A validation error is fatal if the property was required,
        // e.g. if someone entered a non-integer node_id, or an invalid
        // internal RPC address.
        if (!ok && prop->is_required()) {
            throw std::invalid_argument(fmt::format(
              "Property {} is required and has invalid value", name));
        }
    }

    return errors;
}

/**
 *
 * @param filter optional callback for filtering out config properties.
 *               callback should return false to exclude a property.
 */
void config_store::to_json(
  json::Writer<json::StringBuffer>& w,
  redact_secrets redact,
  std::optional<std::function<bool(base_property&)>> filter) const {
    w.StartObject();

    for (const auto& [name, property] : _properties) {
        if (property->get_visibility() == visibility::deprecated) {
            continue;
        }

        if (filter && !filter.value()(*property)) {
            continue;
        }

        w.Key(name.data(), name.size());
        property->to_json(w, redact);
    }

    w.EndObject();
}

void config_store::to_json_for_metrics(json::Writer<json::StringBuffer>& w) {
    w.StartObject();

    for (const auto& [name, property] : _properties) {
        if (property->get_visibility() == visibility::deprecated) {
            continue;
        }

        if (property->type_name() == "boolean") {
            w.Key(name.data(), name.size());
            property->to_json(w, redact_secrets::yes);
            continue;
        }

        if (property->is_nullable()) {
            w.Key(name.data(), name.size());
            w.String(property->is_default() ? "default" : "[value]");
            continue;
        }

        if (!property->enum_values().empty()) {
            w.Key(name.data(), name.size());
            property->to_json(w, redact_secrets::yes);
            continue;
        }
    }

    w.EndObject();
}

std::set<std::string_view> config_store::property_names() const {
    std::set<std::string_view> result;
    for (const auto& i : _properties) {
        result.insert(i.first);
    }

    return result;
}

std::set<std::string_view> config_store::property_aliases() const {
    std::set<std::string_view> result;
    for (const auto& i : _aliases) {
        result.insert(i.first);
    }

    return result;
}

std::set<std::string_view> config_store::property_names_and_aliases() const {
    auto all = property_names();
    all.merge(property_aliases());
    return all;
}

std::ostream& operator<<(std::ostream& o, const config::config_store& c) {
    o << "{ ";
    c.for_each([&o](const auto& property) { o << property << " "; });
    o << "}";
    return o;
}

void config_store::notify_original_version(legacy_version ov) {
    for (const auto& [name, property] : _properties) {
        property->notify_original_version(ov);
    }
}

YAML::Node to_yaml(const config_store& cfg, redact_secrets redact) {
    json::StringBuffer buf;
    json::Writer<json::StringBuffer> writer(buf);
    cfg.to_json(writer, redact);
    return YAML::Load(buf.GetString());
}
}; // namespace config
