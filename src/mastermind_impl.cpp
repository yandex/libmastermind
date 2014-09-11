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
		int expired_time,
		std::string worker_name
		)
	: m_logger(logger)
	, m_remotes(remotes)
	, m_next_remote(0)
	, m_cache_path(std::move(cache_path))
	, m_worker_name(std::move(worker_name))
	, m_group_info_update_period(group_info_update_period)
	, m_done(false)
{
	deserialize(expired_time);

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
			g.wait_for(std::chrono::seconds(4));
			if (g.ready() == false){
				COCAINE_LOG_ERROR(
					m_logger,
					"libmastermind: reconnect: cannot get mastermind-service in 4 seconds from %s:%d",
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

bool mastermind_t::data::collect_group_weights() {
	try {
		metabalancer_groups_info_t::namespaces_t resp;
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
			}
		}

		auto cache = m_metabalancer_groups_info.create(std::move(resp));
		m_metabalancer_groups_info.swap(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_group_weights: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_bad_groups() {
	try {
		auto cache = m_bad_groups.create();
		enqueue("get_bad_groups", "", *cache);
		m_bad_groups.swap(cache);
		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_bad_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_symmetric_groups() {
	try {
		std::vector<std::vector<int>> sym_groups;
		enqueue("get_symmetric_groups", "", sym_groups);
		auto cache = m_symmetric_groups.create();

		for (auto it = sym_groups.begin(); it != sym_groups.end(); ++it) {
			for (auto ve = it->begin(); ve != it->end(); ++ve) {
				(*cache)[*ve] = *it;
			}
		}

		for (auto it = m_bad_groups.cache->begin(); it != m_bad_groups.cache->end(); ++it) {
			for (auto ve = it->begin(); ve != it->end(); ++ve) {
				(*cache)[*ve] = *it;
			}
		}

		m_symmetric_groups.swap(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_symmetric_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_cache_groups() {
	try {
		std::vector<std::pair<std::string, std::vector<int>>> cache_groups;
		enqueue("get_cached_keys", "", cache_groups);

		auto cache = m_cache_groups.create();

		for (auto it = cache_groups.begin(); it != cache_groups.end(); ++it) {
			cache->insert(*it);
		}

		m_cache_groups.swap(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_cache_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_namespaces_settings() {
	try {
		auto cache = m_namespaces_settings.create();
		enqueue("get_namespaces_settings", "", *cache);
		m_namespaces_settings.swap(cache);
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_namespaces_settings: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_metabalancer_info() {
	try {
		auto cache = m_metabalancer_info.create();
		typedef std::vector<std::map<std::string, std::string>> arg_type;
		arg_type arg;
		arg.push_back(std::map<std::string, std::string>());
		enqueue("get_couples_list", arg, *cache);
		m_metabalancer_info.swap(cache);
		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_metabalancer_info: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_namespaces_statistics() {
	try {
		auto cache = m_namespaces_statistics.create();
		enqueue("get_namespaces_statistics", "", *cache);
		m_namespaces_statistics.swap(cache);
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

		auto cache = m_elliptics_remotes.create();
		*cache = std::move(remotes);

		m_elliptics_remotes.swap(cache);
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_elliptics_remotes");
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
		spent_time_printer_t helper("collect_group_weights", m_logger);
		collect_group_weights();
	}
	{
		spent_time_printer_t helper("collect_bad_groups", m_logger);
		collect_bad_groups();
	}
	{
		spent_time_printer_t helper("collect_symmetric_groups", m_logger);
		collect_symmetric_groups();
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

		if (m_cache_update_callback) {
			m_cache_update_callback();
		}

		tm = timeout;
		do {
			tm = m_weight_cache_condition_variable.wait_for(lock,
															std::chrono::seconds(
																m_group_info_update_period));
		} while(tm == no_timeout && m_done == false);
	} while(m_done == false);
}

void mastermind_t::data::serialize() {
	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, std::make_tuple(
		  *m_bad_groups.cache
		, *m_symmetric_groups.cache
		, *m_cache_groups.cache
		, m_metabalancer_groups_info.cache->data()
		, *m_namespaces_settings.cache
		, *m_metabalancer_info.cache
		, *m_namespaces_statistics.cache
		, *m_elliptics_remotes.cache
	));
	std::ofstream output(m_cache_path.c_str());
	std::copy(sbuf.data(), sbuf.data() + sbuf.size(), std::ostreambuf_iterator<char>(output));
}

void mastermind_t::data::deserialize(int expired_time) {
	if (expired_time) {
		struct stat stats;
		if (stat(m_cache_path.c_str(), &stats)) {
			COCAINE_LOG_ERROR(m_logger, "cannot get stats of cache file");
			return;
		}

		auto current_time = time(0);
		auto cache_life_time = current_time - stats.st_mtime;

		if (cache_life_time > expired_time) {
			COCAINE_LOG_ERROR(m_logger, "cache file is not used, it is too old");
			return;
		}
	}

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
		msgpack::object obj = msg.get();

		typedef std::tuple<
			 std::vector<std::vector<int>>
			, std::map<int, std::vector<int>>
			, std::map<std::string, std::vector<int>>
			, metabalancer_groups_info_t::namespaces_t
			, std::vector<namespace_settings_t>
			, mastermind::metabalancer_info_t
			, namespaces_statistics_t
			, std::vector<std::string>
			> cache_type;
		cache_type ct;
		obj.convert(&ct);
		metabalancer_groups_info_t::namespaces_t namespaces;
		std::tie(
			  *m_bad_groups.cache
			, *m_symmetric_groups.cache
			, *m_cache_groups.cache
			, namespaces
			, *m_namespaces_settings.cache
			, *m_metabalancer_info.cache
			, *m_namespaces_statistics.cache
			, *m_elliptics_remotes.cache
			) = std::move(ct);
		m_metabalancer_groups_info.cache = std::make_shared<metabalancer_groups_info_t>(std::move(namespaces));
	} catch (const std::exception &ex) {
		COCAINE_LOG_WARNING(m_logger, "libmastermind: deserialize: cannot read libmastermind.cache: %s", ex.what());
	} catch (...) {
		COCAINE_LOG_WARNING(m_logger, "libmastermind: deserialize: cannot read libmastermind.cache");
	}
}

void mastermind_t::data::cache_force_update() {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;
	collect_info_loop_impl();
}

void mastermind_t::data::set_update_cache_callback(const std::function<void (void)> &callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;

	m_cache_update_callback = callback;
}

} // namespace mastermind
