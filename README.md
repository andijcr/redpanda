results from [ClangBuildAnalyzer](https://github.com/aras-p/ClangBuildAnalyzer)
```
**** Time summary:
Compilation (1294 times):
  Parsing (frontend):        13242.7 s
  Codegen & opts (backend):  20338.9 s

**** Files that took longest to parse (compiler frontend):
 65837 ms: vbuild/release/clang/src//v/redpanda/CMakeFiles/v_application.dir/application.cc.o
 57546 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/members_manager.cc.o
 55681 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/controller.cc.o
 52867 ms: vbuild/release/clang/src//v/coproc/tests/CMakeFiles/coproc_fixture_unstable_rpunit.dir/partition_movement_tests.cc.o
 51336 ms: vbuild/release/clang/src//v/coproc/tests/CMakeFiles/coproc_fixture_unstable_rpunit.dir/event_listener_tests.cc.o
 51169 ms: vbuild/release/clang/src//v/archival/tests/CMakeFiles/test_archival_service_rpunit.dir/retention_strategy_test.cc.o
 50428 ms: vbuild/release/clang/src//v/coproc/tests/CMakeFiles/coproc_fixture_unstable_rpunit.dir/autocreate_topic_tests.cc.o
 49362 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/service.cc.o
 49111 ms: vbuild/release/clang/src//v/coproc/tests/CMakeFiles/coproc_fixture_unstable_rpunit.dir/fixtures/coproc_bench_fixture.cc.o
 48538 ms: vbuild/release/clang/src//v/kafka/client/test/CMakeFiles/test_kafka_client_fixture_rpunit.dir/reconnect.cc.o

**** Files that took longest to codegen (compiler backend):
381210 ms: vbuild/release/clang/src//v/compat/CMakeFiles/v_compat.dir/run.cc.o
337741 ms: vbuild/release/clang/src//v/kafka/protocol/CMakeFiles/v_kafka_protocol.dir/schemata/protocol.cc.o
309134 ms: vbuild/release/clang/src//v/cluster/tests/CMakeFiles/test_serialization_rt_test_rpunit.dir/serialization_rt_test.cc.o
274570 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/topics_frontend.cc.o
232095 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/members_manager.cc.o
212421 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/cluster_discovery.cc.o
211970 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/tx_gateway_frontend.cc.o
205138 ms: vbuild/release/clang/src//v/cluster/CMakeFiles/v_cluster.dir/service.cc.o
199667 ms: vbuild/release/clang/src//v/rpc/test/CMakeFiles/rpcgenerator_cycling_rpunit.dir/rpc_gen_cycling_test.cc.o
199415 ms: vbuild/release/clang/src//v/kafka/client/CMakeFiles/v_kafka_client.dir/client.cc.o

**** Templates that took longest to instantiate:
215612 ms: fmt::format_to<seastar::internal::log_buf::inserter_iterator, unsign... (589 times, avg 366 ms)
213079 ms: fmt::vformat_to<seastar::internal::log_buf::inserter_iterator, 0> (589 times, avg 361 ms)
199794 ms: fmt::v8::detail::vformat_to<char> (553 times, avg 361 ms)
183141 ms: serde::read_nested<cluster::topic_properties> (568 times, avg 322 ms)
167762 ms: std::__function::__func<(lambda at /home/andrea/redpanda/redpanda/vb... (4929 times, avg 34 ms)
143197 ms: std::function<seastar::metrics::impl::metric_value ()>::function<(la... (1643 times, avg 87 ms)
141483 ms: std::__function::__value_func<seastar::metrics::impl::metric_value (... (1643 times, avg 86 ms)
140202 ms: std::__function::__value_func<seastar::metrics::impl::metric_value (... (1643 times, avg 85 ms)
132663 ms: seastar::smp::submit_to<(lambda at /home/andrea/redpanda/redpanda/vb... (2342 times, avg 56 ms)
 97959 ms: std::__function::__alloc_func<(lambda at /home/andrea/redpanda/redpa... (4929 times, avg 19 ms)
 96695 ms: std::basic_regex<char>::basic_regex (321 times, avg 301 ms)
 96413 ms: std::basic_regex<char>::__init<const char *> (319 times, avg 302 ms)
 96228 ms: std::basic_regex<char>::__parse<const char *> (319 times, avg 301 ms)
 95751 ms: fmt::v8::detail::vformat_to(buffer<char> &, basic_string_view<char>,... (553 times, avg 173 ms)
 81651 ms: std::basic_regex<char>::__parse_ecma_exp<const char *> (319 times, avg 255 ms)
 81137 ms: std::basic_regex<char>::__parse_alternative<const char *> (319 times, avg 254 ms)
 81095 ms: std::basic_regex<char>::__parse_term<const char *> (319 times, avg 254 ms)
 73750 ms: fmt::visit_format_arg<fmt::v8::detail::arg_formatter<char> &, fmt::b... (553 times, avg 133 ms)
 62070 ms: std::__function::__func<(lambda at /home/andrea/redpanda/redpanda/vb... (3286 times, avg 18 ms)
 47478 ms: std::basic_regex<char>::__parse_atom<const char *> (319 times, avg 148 ms)
 46641 ms: std::__function::__func<(lambda at /home/andrea/redpanda/redpanda/vb... (1347 times, avg 34 ms)
 44500 ms: std::__function::__func<(lambda at /home/andrea/redpanda/redpanda/vb... (1347 times, avg 33 ms)
 43165 ms: cluster::ntp_callbacks<seastar::noncopyable_function<void (detail::b... (117 times, avg 368 ms)
 42057 ms: seastar::net::ipv4_l4<seastar::net::ip_protocol_num::icmp>::register... (449 times, avg 93 ms)
 41491 ms: fmt::v8::detail::write<char, fmt::appender, float, 0> (1106 times, avg 37 ms)
 40509 ms: seastar::net::arp_for<seastar::net::ipv4>::learn (449 times, avg 90 ms)
 39907 ms: std::function<std::optional<seastar::net::ipv4_traits::l4packet> ()>... (449 times, avg 88 ms)
 39463 ms: std::__function::__value_func<std::optional<seastar::net::ipv4_trait... (449 times, avg 87 ms)
 39331 ms: serde::read_nested<unsigned char> (1088 times, avg 36 ms)
 39138 ms: std::__function::__value_func<std::optional<seastar::net::ipv4_trait... (449 times, avg 87 ms)

**** Template sets that took longest to instantiate:
655277 ms: fmt::format_to<$> (29868 times, avg 21 ms)
600700 ms: std::tuple<$> (87896 times, avg 6 ms)
484916 ms: std::tie<$> (34292 times, avg 14 ms)
432383 ms: seastar::logger::log<$> (24955 times, avg 17 ms)
428750 ms: std::function<$>::function<$> (5069 times, avg 84 ms)
424564 ms: std::__function::__value_func<$>::__value_func<$> (5069 times, avg 83 ms)
390861 ms: seastar::logger::error<$> (22049 times, avg 17 ms)
356613 ms: std::forward_as_tuple<$> (39391 times, avg 9 ms)
350448 ms: fmt::format_arg_store<$>::format_arg_store<$> (48540 times, avg 7 ms)
348521 ms: std::__function::__func<$>::__func (5069 times, avg 68 ms)
340947 ms: std::optional<$> (50575 times, avg 6 ms)
336857 ms: rpc::transport::send_typed<$> (812 times, avg 414 ms)
331182 ms: std::unique_ptr<$> (69601 times, avg 4 ms)
320171 ms: fmt::make_format_args<$> (36516 times, avg 8 ms)
297695 ms: std::vector<$> (64504 times, avg 4 ms)
296641 ms: rpc::transport::send_typed_versioned<$> (812 times, avg 365 ms)
292982 ms: std::__function::__alloc_func<$>::__alloc_func (15207 times, avg 19 ms)
263773 ms: std::allocator_traits<$> (157765 times, avg 1 ms)
256509 ms: seastar::future<$>::then<$> (22960 times, avg 11 ms)
246871 ms: seastar::internal::call_then_impl<$>::run<$> (22960 times, avg 10 ms)
240004 ms: seastar::future<$>::then_impl<$> (22960 times, avg 10 ms)
237778 ms: std::__compressed_pair<$>::__compressed_pair<$> (137040 times, avg 1 ms)
234816 ms: std::pair<$> (57071 times, avg 4 ms)
215389 ms: fmt::vformat_to<$> (1240 times, avg 173 ms)
210152 ms: std::tuple<$>::tuple<$> (77812 times, avg 2 ms)
200663 ms: std::decay<$> (178142 times, avg 1 ms)
199794 ms: fmt::v8::detail::vformat_to<$> (553 times, avg 361 ms)
198318 ms: fmt::basic_format_args<$>::basic_format_args<$> (45484 times, avg 4 ms)
195408 ms: absl::container_internal::raw_hash_set<$>::find<$> (6095 times, avg 32 ms)
194110 ms: fmt::print<$> (12782 times, avg 15 ms)

**** Functions that took longest to compile:
 10877 ms: kafka_client_fixture_consumer_group::fixture_test() (/home/andrea/redpanda/redpanda/src/v/kafka/client/test/consumer_group.cc)
  7006 ms: serde_reflection_roundtrip::do_run_test_case() (/home/andrea/redpanda/redpanda/src/v/cluster/tests/serialization_rt_test.cc)
  5942 ms: partition_iterator::do_run_test_case() (/home/andrea/redpanda/redpanda/src/v/kafka/server/tests/fetch_test.cc)
  5311 ms: cluster::controller_service_base<rpc::default_message_codec>::setup_... (/home/andrea/redpanda/redpanda/src/v/cluster/service.cc)
  4585 ms: config::configuration::configuration() (/home/andrea/redpanda/redpanda/src/v/config/configuration.cc)
  4323 ms: kafka::report_broker_config(kafka::describe_configs_resource const&,... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/describe_configs.cc)
  3328 ms: kafka::handler_template<kafka::describe_configs_api, (short)0, (shor... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/describe_configs.cc)
  3014 ms: kafka::handler_template<kafka::offset_fetch_api, (short)1, (short)7,... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/offset_fetch.cc)
  2821 ms: kafka::handler_template<kafka::offset_fetch_api, (short)1, (short)7,... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/offset_fetch.cc)
  2719 ms: write_and_read_value_test::do_run_test_case() (/home/andrea/redpanda/redpanda/src/v/kafka/server/tests/read_write_roundtrip_test.cc)
  2649 ms: redpanda_thread_fixture_list_offsets_by_time::fixture_test() (/home/andrea/redpanda/redpanda/src/v/kafka/server/tests/list_offsets_test.cc)
  2456 ms: kafka::alter_broker_configuartion(kafka::request_context&, std::__1:... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/incremental_alter_configs.cc)
  2374 ms: std::__1::vector<kafka::api_versions_response_key, std::__1::allocat... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/api_versions.cc)
  2302 ms: pandaproxy::schema_registry::get_schema_registry_routes(seastar::gat... (/home/andrea/redpanda/redpanda/src/v/pandaproxy/schema_registry/service.cc)
  2123 ms: kafka::produce_topic(kafka::produce_ctx&, kafka::topic_produce_data&) (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/produce.cc)
  2079 ms: kafka::handler_template<kafka::create_topics_api, (short)0, (short)6... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/create_topics.cc)
  2063 ms: configuration_backward_compatibility_test::do_run_test_case() (/home/andrea/redpanda/redpanda/src/v/raft/tests/configuration_serialization_test.cc)
  2059 ms: net::client_probe::setup_metrics(seastar::metrics::metric_groups&, s... (/home/andrea/redpanda/redpanda/src/v/net/probes.cc)
  2002 ms: cluster::replicated_partition_probe::setup_public_metrics(model::ntp... (/home/andrea/redpanda/redpanda/src/v/cluster/partition_probe.cc)
  1998 ms: kafka::alter_broker_configuartion(kafka::request_context&, std::__1:... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/incremental_alter_configs.cc)
  1952 ms: cluster::tx_gateway_frontend::do_add_partition_to_tx(seastar::shared... (/home/andrea/redpanda/redpanda/src/v/cluster/tx_gateway_frontend.cc)
  1947 ms: kafka::client::client::produce_records(model::topic, std::__1::vecto... (/home/andrea/redpanda/redpanda/src/v/kafka/client/client.cc)
  1923 ms: kafka::handler_template<kafka::describe_acls_api, (short)0, (short)1... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/describe_acls.cc)
  1875 ms: kafka::get_topic_metadata(kafka::request_context&, kafka::metadata_r... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/metadata.cc)
  1858 ms: storage::probe::setup_metrics(model::ntp const&) (/home/andrea/redpanda/redpanda/src/v/storage/probe.cc)
  1854 ms: kafka::handler_template<kafka::create_partitions_api, (short)0, (sho... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/create_partitions.cc)
  1830 ms: cluster::tx_gateway_service_base<rpc::default_message_codec>::setup_... (/home/andrea/redpanda/redpanda/src/v/cluster/tx_gateway.cc)
  1823 ms: kafka::handler_template<kafka::alter_configs_api, (short)0, (short)1... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/alter_configs.cc)
  1821 ms: kafka::handler_template<kafka::offset_fetch_api, (short)1, (short)7,... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/offset_fetch.cc)
  1811 ms: kafka::alter_broker_configuartion(kafka::request_context&, std::__1:... (/home/andrea/redpanda/redpanda/src/v/kafka/server/handlers/incremental_alter_configs.cc)

**** Function sets that took longest to compile / optimize:
262107 ms: void fmt::v8::detail::vformat_to<$>(fmt::v8::detail::buffer<$>&, fmt... (477 times, avg 549 ms)
236401 ms: char const* fmt::v8::detail::parse_replacement_field<$>(char const*,... (477 times, avg 495 ms)
218608 ms: fmt::v8::appender fmt::v8::detail::write_int_noinline<$>(fmt::v8::ap... (1431 times, avg 152 ms)
215601 ms: fmt::v8::appender fmt::v8::detail::do_write_float<$>(fmt::v8::append... (954 times, avg 225 ms)
134098 ms: seastar::continuation<$>::run_and_dispose() (7259 times, avg 18 ms)
117293 ms: fmt::v8::appender fmt::v8::detail::do_write_float<$>(fmt::v8::append... (477 times, avg 245 ms)
 63281 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (359 times, avg 176 ms)
 54126 ms: serde::header serde::read_header<$>(iobuf_parser&, unsigned long) (761 times, avg 71 ms)
 47952 ms: config::property<$>::print(std::__1::basic_ostream<$>&) const (945 times, avg 50 ms)
 33682 ms: char const* fmt::v8::detail::do_parse_arg_id<$>(char const*, char co... (954 times, avg 35 ms)
 33612 ms: kafka::handler_template<$>::handle(kafka::request_context, seastar::... (49 times, avg 685 ms)
 33508 ms: config::property<$>::set_value(YAML::Node) (945 times, avg 35 ms)
 32542 ms: auto fmt::v8::formatter<$>::format<$>(seastar::basic_sstring<$> cons... (235 times, avg 138 ms)
 31406 ms: decltype(fp.begin()) fmt::v8::formatter<$>::parse<$>(fmt::v8::basic_... (475 times, avg 66 ms)
 30247 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (186 times, avg 162 ms)
 28839 ms: config::property<$>::validate(YAML::Node) const (945 times, avg 30 ms)
 24128 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (152 times, avg 158 ms)
 22950 ms: YAML::convert<$>::decode(YAML::Node const&, std::__1::vector<$>&) (269 times, avg 85 ms)
 22608 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (138 times, avg 163 ms)
 22305 ms: config::config_store::read_yaml(YAML::Node const&, std::__1::set<$>) (90 times, avg 247 ms)
 20033 ms: config::property<$>::set_value(std::__1::any) (945 times, avg 21 ms)
 19728 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (110 times, avg 179 ms)
 18774 ms: seastar::smp_message_queue::async_work_item<$>::run_and_dispose() (582 times, avg 32 ms)
 18526 ms: kafka::handler_template<$>::handle(kafka::request_context, seastar::... (17 times, avg 1089 ms)
 17143 ms: seastar::noncopyable_function<$>::direct_vtable_for<$>::call(seastar... (1754 times, avg 9 ms)
 16608 ms: compat::corpus_helper<$>::check(rapidjson::GenericDocument<$>) (98 times, avg 169 ms)
 15780 ms: fmt::v8::basic_memory_buffer<$>::grow(unsigned long) (978 times, avg 16 ms)
 15584 ms: fmt::v8::appender fmt::v8::detail::fallback_formatter<$>::format<$>(... (90 times, avg 173 ms)
 15422 ms: redpanda_thread_fixture::configure(detail::base_named_type<int, mode... (51 times, avg 302 ms)
 15391 ms: redpanda_thread_fixture::redpanda_thread_fixture(detail::base_named_... (51 times, avg 301 ms)

*** Expensive headers:
1362867 ms: /home/andrea/redpanda/redpanda/src/v/cluster/types.h (included 284 times, avg 4798 ms), included via:
  bootstrap_service.cc.o bootstrap_service.h bootstrap_types.h  (12499 ms)
  group_manager.cc.o group_manager.h  (12298 ms)
  consensus.cc.o consensus.h feature_table.h  (12274 ms)
  tx_gateway.cc.o tx_gateway.h rm_group_proxy.h  (12180 ms)
  controller_api_tests.cc.o controller_api.h  (11650 ms)
  feature_table.cc.o feature_table.h  (11623 ms)
  ...

982961 ms: /home/andrea/redpanda/redpanda/src/v/model/fundamental.h (included 530 times, avg 1854 ms), included via:
  state_machine.cc.o state_machine.h  (5955 ms)
  opfuzz.cc.o opfuzz.h  (5364 ms)
  offset_translator_state.cc.o offset_translator_state.h  (5325 ms)
  heartbeat_manager.cc.o heartbeat_manager.h metadata.h  (5266 ms)
  log_gap_analysis.cc.o log_gap_analysis.h  (5246 ms)
  offset_translator_tests.cc.o  (5186 ms)
  ...

972169 ms: /home/andrea/redpanda/redpanda/src/v/cluster/commands.h (included 204 times, avg 4765 ms), included via:
  metadata_dissemination_test.cc.o metadata_cache.h members_table.h  (13039 ms)
  members_table.cc.o members_table.h  (12404 ms)
  data_policy_manager.cc.o data_policy_manager.h  (12283 ms)
  reconciliation_backend.cc.o reconciliation_backend.h topic_table.h  (12245 ms)
  members_frontend.cc.o members_frontend.h cluster_utils.h controller_stm.h bootstrap_backend.h  (12143 ms)
  metadata_cache.cc.o metadata_cache.h members_table.h  (12101 ms)
  ...

911490 ms: /home/andrea/redpanda/redpanda/src/v/bytes/iobuf.h (included 583 times, avg 1563 ms), included via:
  describe_acls.cc.o describe_acls.h describe_acls.h  (3782 ms)
  log_segment_reader_test.cc.o record.h  (3356 ms)
  members_frontend.cc.o members_frontend.h cluster_utils.h controller_stm.h bootstrap_backend.h commands.h iobuf_parser.h bytes.h  (3306 ms)
  state_machine.cc.o state_machine.h fundamental.h  (3218 ms)
  types.cc.o types.h  (3063 ms)
  snapshot.cc.o snapshot.h  (3014 ms)
  ...

852729 ms: /home/andrea/redpanda/redpanda/src/v/cluster/bootstrap_backend.h (included 178 times, avg 4790 ms), included via:
  members_frontend.cc.o members_frontend.h cluster_utils.h controller_stm.h  (14786 ms)
  cluster_utils.cc.o cluster_utils.h controller_stm.h  (14278 ms)
  partition_allocator_tests.cc.o cluster_utils.h controller_stm.h  (14179 ms)
  feature_barrier_test.cc.o feature_manager.h controller_stm.h  (14072 ms)
  create_partitions_test.cc.o rebalancing_tests_fixture.h members_frontend.h cluster_utils.h controller_stm.h  (13914 ms)
  data_policy_frontend.cc.o data_policy_frontend.h controller_stm.h  (13610 ms)
  ...

825824 ms: /home/andrea/redpanda/redpanda/src/v/config/configuration.h (included 396 times, avg 2085 ms), included via:
  cluster_config_schema_util.cc.o cluster_config_schema_util.h  (10336 ms)
  persisted_stm.cc.o persisted_stm.h  (9362 ms)
  tm_stm_cache.cc.o tm_stm_cache.h persisted_stm.h  (8907 ms)
  tls_config_convert_test.cc.o  (8903 ms)
  partition_balancer_rpc_handler.cc.o partition_balancer_rpc_handler.h partition_balancer_rpc_service.h  (8855 ms)
  tm_stm_tests.cc.o tm_stm.h persisted_stm.h  (8793 ms)
  ...

817423 ms: /home/andrea/redpanda/redpanda/src/v/serde/serde.h (included 544 times, avg 1502 ms), included via:
  fuzz.cc.o generated_structs.h  (4592 ms)
  dns.cc.o dns.h unresolved_address.h  (4536 ms)
  bench.cc.o  (4474 ms)
  license_test.cc.o license.h  (4225 ms)
  license.cc.o license.h  (3758 ms)
  http_imposter.cc.o http_imposter.h unresolved_address.h  (3563 ms)
  ...

711735 ms: /home/andrea/redpanda/redpanda/src/v/kafka/server/request_context.h (included 127 times, avg 5604 ms), included via:
  describe_log_dirs.cc.o describe_log_dirs.h handler.h  (12771 ms)
  protocol_utils.cc.o protocol_utils.h  (12495 ms)
  sasl_handshake.cc.o sasl_handshake.h handler.h  (12065 ms)
  delete_topics.cc.o delete_topics.h handler.h  (12041 ms)
  sync_group.cc.o sync_group.h handler.h  (12040 ms)
  init_producer_id.cc.o init_producer_id.h handler.h  (11933 ms)
  ...

666020 ms: /home/andrea/redpanda/redpanda/src/v/config/bounded_property.h (included 398 times, avg 1673 ms), included via:
  cluster_config_schema_util.cc.o cluster_config_schema_util.h configuration.h  (9669 ms)
  persisted_stm.cc.o persisted_stm.h configuration.h  (8918 ms)
  tls_config_convert_test.cc.o configuration.h  (8475 ms)
  tm_stm_cache.cc.o tm_stm_cache.h persisted_stm.h configuration.h  (8444 ms)
  tm_stm_tests.cc.o tm_stm.h persisted_stm.h configuration.h  (8329 ms)
  partition_balancer_rpc_handler.cc.o partition_balancer_rpc_handler.h partition_balancer_rpc_service.h configuration.h  (8313 ms)
  ...

665477 ms: /home/andrea/redpanda/redpanda/src/v/config/property.h (included 411 times, avg 1619 ms), included via:
  segment_size_jitter_test.cc.o log_manager.h  (8637 ms)
  produce_partition.cc.o produce_partition.h configuration.h config_store.h  (8278 ms)
  storage_resources.cc.o storage_resources.h  (8169 ms)
  request_auth.cc.o request_auth.h  (8091 ms)
  recovery_memory_quota.cc.o recovery_memory_quota.h  (8089 ms)
  conn_quota.cc.o conn_quota.h  (7941 ms)
  ...

```
