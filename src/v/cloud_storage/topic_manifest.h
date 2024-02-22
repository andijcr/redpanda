/*
 * Copyright 2022 Redpanda Data, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/redpanda-data/redpanda/blob/master/licenses/rcl.md
 */

#pragma once

#include "cloud_storage/base_manifest.h"
#include "cloud_storage/types.h"
#include "cluster/types.h"
#include "json/document.h"

#include <optional>

namespace cloud_storage {

class topic_manifest_handler;
class topic_manifest final : public base_manifest {
public:
    constexpr static int32_t first_version = 1;
    // This version introduces the serialization of
    // cluster::topic_configuration, with all its field
    constexpr static int32_t cluster_topic_configuration_version = 2;

    // Version used to identify the current serialization format and the maximum
    // version which can be deserialized from
    constexpr static int32_t current_version
      = cluster_topic_configuration_version;

    /// Create manifest for specific ntp
    explicit topic_manifest(
      const cluster::topic_configuration& cfg, model::initial_revision_id rev);

    /// Create empty manifest that supposed to be updated later
    topic_manifest();

    /// Update manifest file from input_stream (remote set)
    ss::future<> update(ss::input_stream<char> is) override;

    /// Serialize manifest object
    ///
    /// \return asynchronous input_stream with the serialized json
    ss::future<serialized_data_stream> serialize() const override;

    /// Manifest object name in S3
    remote_manifest_path get_manifest_path() const override;

    static remote_manifest_path
    get_topic_manifest_path(model::ns ns, model::topic topic);

    /// Serialize manifest object
    ///
    /// \param out output stream that should be used to output the json
    void serialize(std::ostream& out) const;

    manifest_type get_manifest_type() const override {
        return manifest_type::topic;
    };

    model::initial_revision_id get_revision() const noexcept { return _rev; }

    /// Change topic-manifest revision
    void set_revision(model::initial_revision_id id) noexcept { _rev = id; }

    std::optional<cluster::topic_configuration> const&
    get_topic_config() const noexcept {
        return _topic_config;
    }

    /// return the version of the decoded manifest. useful to decide if to fill
    /// a field that was not encoded in a previous version
    int32_t get_manifest_version() const noexcept { return _manifest_version; }

    bool operator==(const topic_manifest& other) const {
        return std::tie(_topic_config, _rev)
               == std::tie(other._topic_config, other._rev);
    };

private:
    friend struct topic_manifest_tester;
    /// check that all the fields of cluster::topic_properties have a mapping
    bool is_mapping_updated() const noexcept;
    /// Update manifest content from json document that supposed to be generated
    /// from manifest.json file
    void do_update(const topic_manifest_handler& handler);

    std::optional<cluster::topic_configuration> _topic_config;
    model::initial_revision_id _rev;
    int32_t _manifest_version{first_version};
};
} // namespace cloud_storage
