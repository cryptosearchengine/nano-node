#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/property_tree/ptree.hpp>
#include <nano/node/logging.hpp>

boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>> nano::logging::file_sink;
std::atomic_flag nano::logging::logging_already_added{ ATOMIC_FLAG_INIT };

void nano::logging::init (boost::filesystem::path const & application_path_a)
{
	if (!logging_already_added.test_and_set ())
	{
		boost::log::add_common_attributes ();
		if (log_to_cerr ())
		{
			boost::log::add_console_log (std::cerr, boost::log::keywords::format = "[%TimeStamp%]: %Message%");
		}
		auto path = application_path_a / "log";
		file_sink = boost::log::add_file_log (boost::log::keywords::target = path, boost::log::keywords::file_name = path / "log_%Y-%m-%d_%H-%M-%S.%N.log", boost::log::keywords::rotation_size = rotation_size, boost::log::keywords::auto_flush = flush, boost::log::keywords::scan_method = boost::log::sinks::file::scan_method::scan_matching, boost::log::keywords::max_size = max_size, boost::log::keywords::format = "[%TimeStamp%]: %Message%");
	}
}

void nano::logging::release_file_sink ()
{
	if (logging_already_added.test_and_set ())
	{
		boost::log::core::get ()->remove_sink (nano::logging::file_sink);
		nano::logging::file_sink.reset ();
	}
}

nano::error nano::logging::serialize_json (nano::jsonconfig & json) const
{
	json.put ("version", json_version ());
	json.put ("ledger", ledger_logging_value);
	json.put ("ledger_duplicate", ledger_duplicate_logging_value);
	json.put ("vote", vote_logging_value);
	json.put ("network", network_logging_value);
	json.put ("network_message", network_message_logging_value);
	json.put ("network_publish", network_publish_logging_value);
	json.put ("network_packet", network_packet_logging_value);
	json.put ("network_keepalive", network_keepalive_logging_value);
	json.put ("network_node_id_handshake", network_node_id_handshake_logging_value);
	json.put ("node_lifetime_tracing", node_lifetime_tracing_value);
	json.put ("insufficient_work", insufficient_work_logging_value);
	json.put ("log_ipc", log_ipc_value);
	json.put ("bulk_pull", bulk_pull_logging_value);
	json.put ("work_generation_time", work_generation_time_value);
	json.put ("upnp_details", upnp_details_logging_value);
	json.put ("timing", timing_logging_value);
	json.put ("log_to_cerr", log_to_cerr_value);
	json.put ("max_size", max_size);
	json.put ("rotation_size", rotation_size);
	json.put ("flush", flush);
	json.put ("min_time_between_output", min_time_between_log_output.count ());
	return json.get_error ();
}

bool nano::logging::upgrade_json (unsigned version_a, nano::jsonconfig & json)
{
	json.put ("version", json_version ());
	auto upgraded_l (false);
	switch (version_a)
	{
		case 1:
			json.put ("vote", vote_logging_value);
			upgraded_l = true;
		case 2:
			json.put ("rotation_size", rotation_size);
			json.put ("flush", true);
			upgraded_l = true;
		case 3:
			json.put ("network_node_id_handshake", false);
			upgraded_l = true;
		case 4:
			json.put ("upnp_details", "false");
			json.put ("timing", "false");
			upgraded_l = true;
		case 5:
			uintmax_t config_max_size;
			json.get<uintmax_t> ("max_size", config_max_size);
			max_size = std::max (max_size, config_max_size);
			json.put ("max_size", max_size);
			json.put ("log_ipc", true);
			upgraded_l = true;
		case 6:
			json.put ("min_time_between_output", min_time_between_log_output.count ());
			json.erase ("log_rpc");
			upgraded_l = true;
			break;
		case 7:
			break;
		default:
			throw std::runtime_error ("Unknown logging_config version");
			break;
	}
	return upgraded_l;
}

nano::error nano::logging::deserialize_json (bool & upgraded_a, nano::jsonconfig & json)
{
	int version_l;
	if (!json.has_key ("version"))
	{
		version_l = 1;
		json.put ("version", version_l);

		auto work_peers_l (json.get_optional_child ("work_peers"));
		if (!work_peers_l)
		{
			nano::jsonconfig peers;
			json.put_child ("work_peers", peers);
		}
		upgraded_a = true;
	}
	else
	{
		json.get_required<int> ("version", version_l);
	}

	upgraded_a |= upgrade_json (version_l, json);
	json.get<bool> ("ledger", ledger_logging_value);
	json.get<bool> ("ledger_duplicate", ledger_duplicate_logging_value);
	json.get<bool> ("vote", vote_logging_value);
	json.get<bool> ("network", network_logging_value);
	json.get<bool> ("network_message", network_message_logging_value);
	json.get<bool> ("network_publish", network_publish_logging_value);
	json.get<bool> ("network_packet", network_packet_logging_value);
	json.get<bool> ("network_keepalive", network_keepalive_logging_value);
	json.get<bool> ("network_node_id_handshake", network_node_id_handshake_logging_value);
	json.get<bool> ("node_lifetime_tracing", node_lifetime_tracing_value);
	json.get<bool> ("insufficient_work", insufficient_work_logging_value);
	json.get<bool> ("log_ipc", log_ipc_value);
	json.get<bool> ("bulk_pull", bulk_pull_logging_value);
	json.get<bool> ("work_generation_time", work_generation_time_value);
	json.get<bool> ("upnp_details", upnp_details_logging_value);
	json.get<bool> ("timing", timing_logging_value);
	json.get<bool> ("log_to_cerr", log_to_cerr_value);
	json.get<bool> ("flush", flush);
	json.get<uintmax_t> ("max_size", max_size);
	json.get<uintmax_t> ("rotation_size", rotation_size);
	uintmax_t min_time_between_log_output_raw;
	json.get<uintmax_t> ("min_time_between_output", min_time_between_log_output_raw);
	min_time_between_log_output = std::chrono::milliseconds (min_time_between_log_output_raw);
	return json.get_error ();
}

bool nano::logging::ledger_logging () const
{
	return ledger_logging_value;
}

bool nano::logging::ledger_duplicate_logging () const
{
	return ledger_logging () && ledger_duplicate_logging_value;
}

bool nano::logging::vote_logging () const
{
	return vote_logging_value;
}

bool nano::logging::network_logging () const
{
	return network_logging_value;
}

bool nano::logging::network_message_logging () const
{
	return network_logging () && network_message_logging_value;
}

bool nano::logging::network_publish_logging () const
{
	return network_logging () && network_publish_logging_value;
}

bool nano::logging::network_packet_logging () const
{
	return network_logging () && network_packet_logging_value;
}

bool nano::logging::network_keepalive_logging () const
{
	return network_logging () && network_keepalive_logging_value;
}

bool nano::logging::network_node_id_handshake_logging () const
{
	return network_logging () && network_node_id_handshake_logging_value;
}

bool nano::logging::node_lifetime_tracing () const
{
	return node_lifetime_tracing_value;
}

bool nano::logging::insufficient_work_logging () const
{
	return network_logging () && insufficient_work_logging_value;
}

bool nano::logging::log_ipc () const
{
	return network_logging () && log_ipc_value;
}

bool nano::logging::bulk_pull_logging () const
{
	return network_logging () && bulk_pull_logging_value;
}

bool nano::logging::callback_logging () const
{
	return network_logging ();
}

bool nano::logging::work_generation_time () const
{
	return work_generation_time_value;
}

bool nano::logging::upnp_details_logging () const
{
	return upnp_details_logging_value;
}

bool nano::logging::timing_logging () const
{
	return timing_logging_value;
}

bool nano::logging::log_to_cerr () const
{
	return log_to_cerr_value;
}
