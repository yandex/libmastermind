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
	, namespaces_weights(logger, "namespaces_weights")
	, namespaces_states(logger, "namespaces_states")
	, metabalancer_info(logger, "metabalancer_info")
	, namespaces_settings(logger, "namespaces_settings")
	, cache_groups(logger, "cache_groups")
	, namespaces_statistics(logger, "namespaces_statistics")
	, elliptics_remotes(logger, "elliptics_remotes")
	, bad_groups(logger, "bad_groups")
	, symmetric_groups(logger, "symmetric_groups")
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
mastermind_t::data::collect_namespaces_weights() {
	try {
		namespace_weights_t::namespaces_t resp;
		enqueue("get_group_weights", "", resp);

		for (auto ns_it = resp.begin(), ns_end = resp.end(); ns_it != ns_end; ++ns_it) {
			for (auto it = ns_it->second.begin(), end = ns_it->second.end(); it != end; ++it) {
				size_t zero_weight = 0;
				size_t nonzero_weight = 0;

				for (auto cit = it->second.begin(), cend = it->second.end(); cit != cend; ++cit) {
					if (std::get<1>(*cit) == 0) {
						zero_weight += 1;
					} else {
						nonzero_weight += 1;
					}
				}

				std::ostringstream oss;
				oss
					<< "libmastermind: couple-weights-counts:"
					<< " namespace=" << ns_it->first
					<< " groups-in-couple=" << it->first
					<< " nonzero-weight-count=" << nonzero_weight
					<< " zero-weight-count=" << zero_weight;

				auto msg = oss.str();

				COCAINE_LOG_INFO(m_logger, "%s", msg.c_str());

				if (nonzero_weight != 0) {
					namespaces_weights.set(ns_it->first, it->first
							, namespaces_weights.create_value(it->second));
				} else if (zero_weight != 0) {
					std::ostringstream oss;
					oss
						<< "libmastermind: couple-weights-counts:"
						<< " namespace=" << ns_it->first
						<< " only zero-weight couples were received from mastermind";

					auto msg = oss.str();

					COCAINE_LOG_ERROR(m_logger, "%s", msg.c_str());
				} else {
					std::ostringstream oss;
					oss
						<< "libmastermind: couple-weights-counts:"
						<< " namespace=" << ns_it->first
						<< " no couples were received from mastermind";

					auto msg = oss.str();

					COCAINE_LOG_ERROR(m_logger, "%s", msg.c_str());
				}
			}
		}

	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: cannot collect namespaces_weights: %s", ex.what());
	}
}

void
mastermind_t::data::collect_namespaces_states() {
	try {
		auto dynamic_namespaces_states = enqueue("get_namespaces_states").as_object();

		for (auto it = dynamic_namespaces_states.begin(), end = dynamic_namespaces_states.end();
				it != end; ++it) {
			const auto &name = it->first;

			try {
				// TODO: forward real factory
				namespace_state_t::user_settings_factory_t fake_factory;
				auto ns_state = namespaces_states.create_value(name
							, kora::config_t(name, it->second), fake_factory);

				// TODO: check new ns_state is better than the old one
				// auto old_ns_state = namespaces_states.copy(name, 0);
				// if (ns_state is better than old_ns_state) {
				// 	namespaces_states.set(name, 0, ns_state);
				// } else {
				// 	throw std::runtime_error("old namespace_state is better than the new one");
				// }
				namespaces_states.set(name, 0, ns_state);
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
		std::vector<std::pair<std::string, std::vector<int>>> raw_cache_groups;
		enqueue("get_cached_keys", "", raw_cache_groups);

		auto cache = cache_groups.create_value();

		for (auto it = raw_cache_groups.begin(); it != raw_cache_groups.end(); ++it) {
			cache->insert(*it);
		}

		cache_groups.set(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_cache_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_namespaces_settings() {
	try {
		auto cache = namespaces_settings.create_value();
		enqueue("get_namespaces_settings", "", *cache);
		namespaces_settings.set(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_namespaces_settings: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_metabalancer_info() {
	try {
		auto cache = metabalancer_info.create_value();
		typedef std::vector<std::map<std::string, std::string>> arg_type;
		arg_type arg;
		arg.push_back(std::map<std::string, std::string>());
		enqueue("get_couples_list", arg, *cache);
		metabalancer_info.set(cache);
		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_metabalancer_info: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_namespaces_statistics() {
	try {
		auto cache = namespaces_statistics.create_value();
		enqueue("get_namespaces_statistics", "", *cache);
		namespaces_statistics.set(cache);
		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_namespaces_statistics: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_elliptics_remotes() {
	try {
		std::vector<std::tuple<std::string, int, int>> raw_remotes;
		enqueue("get_config_remotes", "", raw_remotes);

		std::vector<std::string> remotes;
		remotes.reserve(raw_remotes.size());

		for (auto it = raw_remotes.begin(), end = raw_remotes.end(); it != end; ++it) {
			std::ostringstream oss;
			oss << std::get<0>(*it) << ':' << std::get<1>(*it) << ':' << std::get<2>(*it);
			remotes.emplace_back(oss.str());
		}

		elliptics_remotes.set(std::move(remotes));
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_elliptics_remotes");
	}
	return false;
}

void
mastermind_t::data::generate_groups_caches() {
	std::vector<std::vector<int>> raw_bad_groups;
	std::map<int, std::vector<int>> raw_symmetric_groups;

	auto cache = metabalancer_info.copy();
	const auto &couple_info_map = cache->couple_info_map;

	for (auto it = couple_info_map.begin(), end = couple_info_map.end(); it != end; ++it) {
		const auto &couple_info = it->second;

		for (auto t_it = couple_info->tuple.begin(), t_end = couple_info->tuple.end();
				t_it != t_end; ++t_it) {
			raw_symmetric_groups.insert(std::make_pair(*t_it, couple_info->tuple));
		}

		if (couple_info->couple_status == couple_info_t::BAD) {
			raw_bad_groups.push_back(couple_info->tuple);
		}
	}

	symmetric_groups.set(raw_symmetric_groups);
	bad_groups.set(raw_bad_groups);
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
		spent_time_printer_t helper("collect_namespaces_weights", m_logger);
		collect_namespaces_weights();
	}
	{
		spent_time_printer_t helper("collect_namespaces_states", m_logger);
		collect_namespaces_states();
	}
	{
		spent_time_printer_t helper("collect_cache_groups", m_logger);
		collect_cache_groups();
	}
	{
		spent_time_printer_t helper("collect_metabalancer_info", m_logger);
		collect_metabalancer_info();
	}
	{
		spent_time_printer_t helper("collect_namespaces_settings", m_logger);
		collect_namespaces_settings();
	}
	{
		spent_time_printer_t helper("collect_namespaces_statistics", m_logger);
		collect_namespaces_statistics();
	}
	{
		spent_time_printer_t helper("collect_elliptics_remotes", m_logger);
		collect_elliptics_remotes();
	}

	cache_expire();
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
	auto preferable_life_time = std::chrono::seconds(m_group_info_update_period / 2);

	cache_is_expired = false;

	cache_is_expired = cache_is_expired ||
		namespaces_weights.expire_if(preferable_life_time, warning_time, expire_time);

	cache_is_expired = cache_is_expired ||
		metabalancer_info.expire_if(preferable_life_time, warning_time, expire_time);

	cache_is_expired = cache_is_expired ||
		namespaces_settings.expire_if(preferable_life_time, warning_time, expire_time);

	cache_groups.expire_if(preferable_life_time, warning_time, expire_time);
	namespaces_statistics.expire_if(preferable_life_time, warning_time, expire_time);
	elliptics_remotes.expire_if(preferable_life_time, warning_time, expire_time);

	generate_groups_caches();

	{
		auto namespaces = namespaces_settings.copy();

		for (auto it = namespaces->begin(), end = namespaces->end(); it != end; ++it) {
			if (it->is_active()) {
				if (!namespaces_weights.exist(it->name(), it->groups_count())) {
					cache_is_expired = true;

					COCAINE_LOG_ERROR(m_logger
							, "cache \"namespaces_weights\":\"%s\"(%d) was not obtained"
							, it->name().c_str(), it->groups_count());
				}
			}
		}
	}
}

void mastermind_t::data::serialize() {
	msgpack::sbuffer sbuffer;
	msgpack::packer<msgpack::sbuffer> packer(sbuffer);

	packer.pack_map(6);

#define PACK_CACHE(name) \
	do { \
		packer.pack(std::string(#name)); \
		packer.pack(name); \
	} while (false)

	PACK_CACHE(namespaces_weights);
	PACK_CACHE(metabalancer_info);
	PACK_CACHE(namespaces_settings);
	PACK_CACHE(cache_groups);
	PACK_CACHE(namespaces_statistics);
	PACK_CACHE(elliptics_remotes);

#undef PACK_CACHE

	std::ofstream output(m_cache_path.c_str());
	std::copy(sbuffer.data(), sbuffer.data() + sbuffer.size()
			, std::ostreambuf_iterator<char>(output));
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

		for (msgpack::object_kv *it = object.via.map.ptr, *it_end = it + object.via.map.size;
				it != it_end; ++it) {
			std::string key;
			it->key.convert(&key);

#define TRY_UNPACK_CACHE(name) \
			if (key == #name) { \
				it->val.convert(&name); \
				continue; \
			}

			TRY_UNPACK_CACHE(namespaces_weights);
			TRY_UNPACK_CACHE(metabalancer_info);
			TRY_UNPACK_CACHE(namespaces_settings);
			TRY_UNPACK_CACHE(cache_groups);
			TRY_UNPACK_CACHE(namespaces_statistics);
			TRY_UNPACK_CACHE(elliptics_remotes);

#undef TRY_UNPACK_CACHE
		}

		cache_expire();
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
