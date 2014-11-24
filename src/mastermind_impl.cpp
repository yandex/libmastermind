/*
	Client library for mastermind
	Copyright (C) 2013-2014 Yandex

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mastermind_impl.hpp"
#include "namespace_p.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mastermind {

mastermind_t::data::data(
		const remotes_t &remotes,
		const std::shared_ptr<cocaine::framework::logger_t> &logger,
		int group_info_update_period,
		std::string cache_path,
		int warning_time_,
		int expire_time_,
		std::string worker_name,
		int enqueue_timeout_,
		int reconnect_timeout_
		)
	: m_logger(logger)
	, m_remotes(remotes)
	, m_next_remote(0)
	, m_cache_path(std::move(cache_path))
	, m_worker_name(std::move(worker_name))
	, cache_groups({std::map<std::string, groups_t>(), kora::dynamic_t::empty_array
			, "cache_groups"})
	, elliptics_remotes({std::vector<std::string>(), kora::dynamic_t::empty_array
			, "elliptics_remotes"})
	, namespaces_settings({"namespaces_settings"})
	, bad_groups({"bad_groups"})
	, fake_groups_info({"fake_groups_info"})
	, m_group_info_update_period(group_info_update_period)
	, m_done(false)
	, warning_time(std::chrono::seconds(warning_time_))
	, expire_time(std::chrono::seconds(expire_time_))
	, enqueue_timeout(enqueue_timeout_)
	, reconnect_timeout(reconnect_timeout_)
	, cache_is_expired(false)
{
	deserialize();

	m_weight_cache_update_thread = std::thread(std::bind(&mastermind_t::data::collect_info_loop, this));
}

mastermind_t::data::~data() {
}

void mastermind_t::data::stop() {
	try {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			(void)lock;
			m_done = true;
			m_weight_cache_condition_variable.notify_one();
		}
		m_weight_cache_update_thread.join();
	} catch (const std::system_error &) {
	}
	m_app.reset();
	m_service_manager.reset();
}

void mastermind_t::data::reconnect() {
	std::lock_guard<std::mutex> lock(m_reconnect_mutex);
	(void) lock;

	size_t end = m_next_remote;
	size_t index = m_next_remote;
	size_t size = m_remotes.size();

	do {
		auto &remote = m_remotes[index];
		try {
			COCAINE_LOG_INFO(m_logger,
					"libmastermind: reconnect: try to connect to locator %s:%d",
					remote.first.c_str(), static_cast<int>(remote.second));

			m_app.reset();
			m_service_manager = cocaine::framework::service_manager_t::create(
				cocaine::framework::service_manager_t::endpoint_t(remote.first, remote.second));

			COCAINE_LOG_INFO(m_logger,
					"libmastermind: reconnect: connected to locator, getting mastermind service");

			auto g = m_service_manager->get_service_async<cocaine::framework::app_service_t>(m_worker_name);
			g.wait_for(reconnect_timeout);
			if (g.ready() == false){
				COCAINE_LOG_ERROR(
					m_logger,
					"libmastermind: reconnect: cannot get mastermind-service in %d milliseconds from %s:%d",
					static_cast<int>(reconnect_timeout.count()),
					remote.first.c_str(), static_cast<int>(remote.second));
				g = decltype(g)();
				m_service_manager.reset();
				index = (index + 1) % size;
				continue;
			}
			m_app = g.get();

			COCAINE_LOG_INFO(m_logger,
					"libmastermind: reconnect: connected to mastermind via locator %s:%d"
					, remote.first.c_str(), static_cast<int>(remote.second));

			m_current_remote = remote;
			m_next_remote = (index + 1) % size;
			return;
		} catch (const cocaine::framework::service_error_t &ex) {
			COCAINE_LOG_ERROR(
				m_logger,
				"libmastermind: reconnect: service_error: %s; host: %s:%d",
				ex.what(), remote.first.c_str(), static_cast<int>(remote.second));
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(
				m_logger,
				"libmastermind: reconnect: %s; host: %s:%d",
				ex.what(), remote.first.c_str(), static_cast<int>(remote.second));
		}

		index = (index + 1) % size;
	} while (index != end);

	m_current_remote = remote_t();
	m_app.reset();
	m_service_manager.reset();
	COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: cannot recconect to any host");
	throw std::runtime_error("reconnect error: cannot reconnect to any host");
}

kora::dynamic_t
mastermind_t::data::enqueue(const std::string &event) {
	return enqueue(event, "");
}

void
mastermind_t::data::collect_namespaces_states() {
	try {
		auto dynamic_namespaces_states = enqueue("get_namespaces_states").as_object();

		for (auto it = dynamic_namespaces_states.begin(), end = dynamic_namespaces_states.end();
				it != end; ++it) {
			const auto &name = it->first;

			try {
				auto ns_state = create_namespaces_states(name, it->second);

				// TODO: check new ns_state is better than the old one
				// auto old_ns_state = namespaces_states.copy(name);
				// if (ns_state is better than old_ns_state) {
				// 	namespaces_states.set(name, ns_state);
				// } else {
				// 	throw std::runtime_error("old namespace_state is better than the new one");
				// }
				namespaces_states.set(name, {std::move(ns_state), std::move(it->second), name});
			} catch (const std::exception &ex) {
				COCAINE_LOG_ERROR(m_logger
						, "libmastermind: cannot update namespace_state for %s: %s"
						, name.c_str(), ex.what());
			}
		}

	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger
				, "libmastermind: cannot process collect_namespaces_states: %s"
				, ex.what());
	}
}

bool mastermind_t::data::collect_cache_groups() {
	try {
		auto raw_cache_groups = enqueue("get_cached_keys");
		auto cache = create_cache_groups("", raw_cache_groups);

		cache_groups.set({std::move(cache), std::move(raw_cache_groups)});
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger
				, "libmastermind: cannot process collect_cache_groups: %s"
				, ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_elliptics_remotes() {
	try {
		auto raw_elliptics_remotes = enqueue("get_config_remotes");
		auto cache = create_elliptics_remotes("", raw_elliptics_remotes);
		elliptics_remotes.set({std::move(cache), std::move(raw_elliptics_remotes)});
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger
				, "libmastermind: cannot process collect_elliptics_remotes: %s"
				, ex.what());
	}
	return false;
}

void mastermind_t::data::collect_info_loop_impl() {
	if (m_logger->verbosity() >= cocaine::logging::info) {
		auto current_remote = m_current_remote;
		std::ostringstream oss;
		oss << "libmastermind: collect_info_loop: begin; current host: ";
		if (current_remote.first.empty()) {
			oss << "none";
		} else {
			oss << current_remote.first << ':' << m_current_remote.second;
		}
		COCAINE_LOG_INFO(m_logger, "%s", oss.str().c_str());
	}

	auto beg_time = std::chrono::system_clock::now();

	{
		spent_time_printer_t helper("collect_namespaces_states", m_logger);
		collect_namespaces_states();
	}
	{
		spent_time_printer_t helper("collect_cache_groups", m_logger);
		collect_cache_groups();
	}
	{
		spent_time_printer_t helper("collect_elliptics_remotes", m_logger);
		collect_elliptics_remotes();
	}

	cache_expire();
	generate_fake_caches();
	serialize();

	auto end_time = std::chrono::system_clock::now();

	if (m_logger->verbosity() >= cocaine::logging::info) {
		auto current_remote = m_current_remote;
		std::ostringstream oss;
		oss << "libmastermind: collect_info_loop: end; current host: ";
		if (current_remote.first.empty()) {
			oss << "none";
		} else {
			oss << current_remote.first << ':' << m_current_remote.second;
		}
		oss
			<< "; spent time: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end_time - beg_time).count()
			<< " milliseconds";
		COCAINE_LOG_INFO(m_logger, "%s", oss.str().c_str());
	}
}

void mastermind_t::data::collect_info_loop() {
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_done) {
		COCAINE_LOG_INFO(m_logger, "libmastermind: have to stop immediately");
		return;
	}

	try {
		reconnect();
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: %s", ex.what());
	}

#if __GNUC_MINOR__ >= 6
	auto no_timeout = std::cv_status::no_timeout;
	auto timeout = std::cv_status::timeout;
	auto tm = timeout;
#else
	bool no_timeout = true;
	bool timeout = false;
	bool tm = timeout;
#endif
	COCAINE_LOG_INFO(m_logger, "libmastermind: collect_info_loop: update period is %d", static_cast<int>(m_group_info_update_period));
	do {
		collect_info_loop_impl();

		process_callbacks();

		tm = timeout;
		do {
			tm = m_weight_cache_condition_variable.wait_for(lock,
															std::chrono::seconds(
																m_group_info_update_period));
		} while(tm == no_timeout && m_done == false);
	} while(m_done == false);
}

void
mastermind_t::data::cache_expire() {
	auto preferable_life_time = std::chrono::seconds(m_group_info_update_period);

	cache_is_expired = false;

	{
		auto cache = cache_groups.copy();
		if (!cache.is_expired() && check_cache_for_expire("cache_groups", cache
					, preferable_life_time, warning_time, expire_time)) {
			cache.set_expire(true);
			cache_groups.set(cache);
		}
	}

	{
		auto cache = elliptics_remotes.copy();
		if (!cache.is_expired() && check_cache_for_expire("elliptics_remotes", cache
					, preferable_life_time, warning_time, expire_time)) {
			cache.set_expire(true);
			elliptics_remotes.set(cache);
		}
	}

	{
		auto cache_map = namespaces_states.copy();

		for (auto it = cache_map.begin(), end = cache_map.end(); it != end; ++it) {
			const auto &name = it->first;
			auto &cache = it->second;

			cache_is_expired = cache_is_expired || cache.is_expired();

			if (check_cache_for_expire("namespaces_states:" + name
						, cache, preferable_life_time, warning_time, expire_time)) {
				cache_is_expired = true;

				if (!cache.is_expired()) {
					cache.set_expire(true);
					namespaces_states.set(name, std::move(cache));
				}
			}
		}
	}
}

void
mastermind_t::data::generate_fake_caches() {
	std::vector<groups_t> raw_bad_groups;
	std::map<group_t, fake_group_info_t> raw_fake_groups_info;
	std::vector<namespace_settings_t> raw_namespaces_settings;

	auto cache = namespaces_states.copy();

	for (auto ns_it = cache.begin(), ns_end = cache.end(); ns_it != ns_end; ++ns_it) {
		if (ns_it->second.is_expired()) {
			continue;
		}

		const auto &states = ns_it->second.get_value();
		const auto &raw_states = ns_it->second.get_raw_value();
		const auto &couples = states.couples.couple_info_map;


		for (auto cim_it = couples.begin(), cim_end = couples.end();
				cim_it != cim_end; ++cim_it) {
			const auto &couple = cim_it->second;
			for (auto it = couple.groups_info_map_iterator.begin()
					, end = couple.groups_info_map_iterator.end();
					it != end; ++it) {
				fake_group_info_t fake_group_info;

				fake_group_info.id = (*it)->second.id;
				fake_group_info.groups = couple.groups;
				fake_group_info.free_effective_space = couple.free_effective_space;
				fake_group_info.ns = states.name;
				fake_group_info.group_status = (*it)->second.status;

				raw_fake_groups_info.insert(std::make_pair(fake_group_info.id, fake_group_info));
			}

			if (couple.status == namespace_state_init_t::data_t::couples_t
					::couple_info_t::status_tag::BAD) {
				raw_bad_groups.emplace_back(couple.groups);
			}
		}

		raw_namespaces_settings.emplace_back(create_namespace_settings(states.name
					, raw_states.as_object()["settings"]));
	}

	bad_groups.set({std::move(raw_bad_groups)});
	fake_groups_info.set({std::move(raw_fake_groups_info)});
	namespaces_settings.set({std::move(raw_namespaces_settings)});
}

void mastermind_t::data::serialize() {
	kora::dynamic_t raw_cache = kora::dynamic_t::empty_object;
	auto &raw_cache_object = raw_cache.as_object();

#define PACK_CACHE(cache) \
	do { \
		raw_cache_object[#cache] = cache.copy().serialize(); \
	} while (false)

	PACK_CACHE(cache_groups);
	PACK_CACHE(elliptics_remotes);

#undef PACK_CACHE

	{
		auto cache = namespaces_states.copy();
		kora::dynamic_t raw_namespaces_states = kora::dynamic_t::empty_object;
		auto &raw_namespaces_states_object = raw_namespaces_states.as_object();

		for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
			raw_namespaces_states_object[it->first] = it->second.serialize();
		}

		raw_cache_object["namespaces_states"] = raw_namespaces_states;
	}

	msgpack::sbuffer sbuffer;
	msgpack::packer<msgpack::sbuffer> packer(sbuffer);

	cocaine::io::type_traits<kora::dynamic_t>::pack(packer, raw_cache);

	std::ofstream output(m_cache_path.c_str());
	std::copy(sbuffer.data(), sbuffer.data() + sbuffer.size()
			, std::ostreambuf_iterator<char>(output));
}

namespace_state_init_t::data_t
mastermind_t::data::create_namespaces_states(const std::string &name
		, const kora::dynamic_t &raw_value) {
	// TODO: forward real factory
	namespace_state_t::user_settings_factory_t fake_factory;
	namespace_state_init_t::data_t ns_state{name
		, kora::config_t(name, raw_value), fake_factory};
	COCAINE_LOG_INFO(m_logger, "libmastermind: namespace_state: %s", ns_state.extract.c_str());
	return ns_state;
}

std::map<std::string, groups_t>
mastermind_t::data::create_cache_groups(const std::string &name
		, const kora::dynamic_t &raw_value) {
	(void) name;

	std::map<std::string, groups_t> result;

	const auto &raw_value_array = raw_value.as_array();

	for (auto p_it = raw_value_array.begin(), p_end = raw_value_array.end();
			p_it != p_end; ++p_it) {
		const auto &pair = p_it->as_array();
		const auto &name = pair[0].to<std::string>();
		const auto &raw_groups = pair[1].as_array();
		groups_t groups;

		for (auto it = raw_groups.begin(), end = raw_groups.end(); it != end; ++it) {
			groups.emplace_back(it->to<group_t>());
		}

		result.insert(std::make_pair(name, groups));
	}

	if (result.empty()) {
		throw std::runtime_error("cache-groups list is empty");
	}

	return result;
}

std::vector<std::string>
mastermind_t::data::create_elliptics_remotes(const std::string &name
		, const kora::dynamic_t &raw_value) {
	(void) name;

	std::vector<std::string> result;

	const auto &raw_value_array = raw_value.as_array();

	for (auto p_it = raw_value_array.begin(), p_end = raw_value_array.end();
			p_it != p_end; ++p_it) {
		const auto &tuple = p_it->as_array();
		const auto &name = tuple[0].to<std::string>();
		const auto &port = tuple[1].to<int>();
		const auto &family = tuple[2].to<int>();

		std::ostringstream oss;
		oss << name << ':' << port << ':' << family;

		result.emplace_back(oss.str());
	}

	if (result.empty()) {
		throw std::runtime_error("elliptics-remotes list is empty");
	}

	return result;
}

namespace_settings_t
mastermind_t::data::create_namespace_settings(const std::string &name
		, const kora::dynamic_t &raw_value) {
	namespace_settings_t::data item;

	kora::config_t config(name, raw_value);

	item.name = name;

	item.groups_count = config.at<int>("groups-count");
	item.success_copies_num = config.at<std::string>("success-copies-num");

	item.auth_key = config.at<std::string>("auth-key", "");

	if (config.has("auth-keys")) {
		const auto &auth_keys_config = config.at("auth-keys");
		item.auth_key_for_write = auth_keys_config.at<std::string>("write", "");
		item.auth_key_for_read = auth_keys_config.at<std::string>("read", "");
	}

	if (config.has("static-couple")) {
		const auto &static_couple_config = config.at("static-couple");

		for (size_t index = 0, size = static_couple_config.size(); index != size; ++index) {
			item.static_couple.emplace_back(static_couple_config.at<int>(index));
		}
	}

	if (config.has("signature")) {
		const auto &signature_config = config.at("signature");
		item.sign_token = signature_config.at<std::string>("token", "");
		item.sign_path_prefix = signature_config.at<std::string>("path_prefix", "");
		item.sign_port = signature_config.at<std::string>("port", "");
	}

	if (config.has("redirect")) {
		const auto &redirect_config = config.at("redirect");
		item.redirect_expire_time = redirect_config.at<int>("expire-time", 0);
		item.redirect_content_length_threshold
			= redirect_config.at<int>("content-length-threshold", -1);
	}

	item.is_active = config.at<bool>("is_active", false);

	if (config.has("features")) {
		const auto &features_config = config.at("features");

		item.can_choose_couple_to_upload
			= features_config.at<bool>("select-couple-to-upload", false);

		if (features_config.has("multipart")) {
			const auto &multipart_features_config = features_config.at("multipart");

			item.multipart_content_length_threshold
				= multipart_features_config.at<int64_t>("content-length-threshold", 0);
		}
	}

	return { std::move(item) };
}

void mastermind_t::data::deserialize() {
	std::string file;
	{
		std::ifstream input(m_cache_path.c_str());
		if (input.is_open() == false) {
			return;
		}
		typedef std::istreambuf_iterator<char> it_t;
		file.assign(it_t(input), it_t());
	}

	try {
		msgpack::unpacked msg;
		msgpack::unpack(&msg, file.data(), file.size());
		msgpack::object object = msg.get();

		kora::dynamic_t raw_cache;

		cocaine::io::type_traits<kora::dynamic_t>::unpack(object, raw_cache);

		auto &raw_cache_object = raw_cache.as_object();

#define TRY_UNPACK_CACHE(cache) \
		do { \
			try { \
				cache.set(cache##_t::cache_type(raw_cache_object[#cache].as_object() \
							, std::bind(&data::create_##cache, this \
								, std::placeholders::_1, std::placeholders::_2) \
							, #cache)); \
			} catch (const std::exception &ex) { \
				COCAINE_LOG_ERROR(m_logger, "libmastermind: cannot deserialize cache %s: %s" \
						, #cache, ex.what()); \
			} \
		} while (false)

		TRY_UNPACK_CACHE(cache_groups);
		TRY_UNPACK_CACHE(elliptics_remotes);

#undef TRY_UNPACK_CACHE

		{
			const auto &raw_namespaces_states = raw_cache_object["namespaces_states"];
			const auto &raw_namespaces_states_object = raw_namespaces_states.as_object();

			for (auto it = raw_namespaces_states_object.begin()
					, end = raw_namespaces_states_object.end();
					it != end; ++it) {
				const auto &name = it->first;

				try {
					namespaces_states.set(name, namespaces_states_t::cache_type(
								it->second.as_object()
								, std::bind(&data::create_namespaces_states, this
									, std::placeholders::_1, std::placeholders::_2)
								, name));
				} catch (const std::exception &ex) {
					COCAINE_LOG_ERROR(m_logger
							, "libmastermind: cannot update namespace_state for %s: %s"
							, name.c_str(), ex.what());
				}
			}
		}

		cache_expire();
		generate_fake_caches();
	} catch (const std::exception &ex) {
		COCAINE_LOG_WARNING(m_logger
				, "libmastermind: cannot deserialize libmastermind cache: %s"
				, ex.what());
	} catch (...) {
		COCAINE_LOG_WARNING(m_logger
				, "libmastermind: cannot deserialize libmastermind cache");
	}
}

void mastermind_t::data::cache_force_update() {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;
	collect_info_loop_impl();
	process_callbacks();
}

void mastermind_t::data::set_update_cache_callback(const std::function<void (void)> &callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;

	m_cache_update_callback = callback;
}

void
mastermind_t::data::set_update_cache_ext1_callback(const std::function<void (bool)> &callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;

	cache_update_ext1_callback = callback;
}

void
mastermind_t::data::process_callbacks() {
	if (m_cache_update_callback) {
		m_cache_update_callback();
	}

	if (cache_update_ext1_callback) {
		cache_update_ext1_callback(cache_is_expired);
	}
}

} // namespace mastermind
