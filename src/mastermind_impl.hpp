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

#ifndef SRC__MASTERMIND_IMPL_HPP
#define SRC__MASTERMIND_IMPL_HPP

#include "libmastermind/mastermind.hpp"
#include "utils.hpp"
#include "cache_p.hpp"
#include "namespace_state_p.hpp"

#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <fstream>
#include <functional>
#include <utility>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/common.hpp>
#include <cocaine/traits/tuple.hpp>

#include <kora/dynamic.hpp>

#include <boost/lexical_cast.hpp>

namespace mastermind {

struct mastermind_t::data {
	data(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time_, int expire_time_, std::string worker_name,
			int enqueue_timeout_, int reconnect_timeout_);
	~data();

	void stop();

	void reconnect();

	template <typename T>
	kora::dynamic_t
	simple_enqueue(const std::string &event, const T &chunk);

	template <typename R, typename T>
	bool simple_enqueue(const std::string &event, const T &chunk, R &result);

	kora::dynamic_t
	enqueue(const std::string &event);

	template <typename T>
	kora::dynamic_t
	enqueue(const std::string &event, const T &chunk);

	template <typename R, typename T>
	void enqueue(const std::string &event, const T &chunk, R &result);

	void
	collect_namespaces_states();

	bool collect_cache_groups();
	bool collect_namespaces_settings();
	bool collect_metabalancer_info();
	bool collect_namespaces_statistics();
	bool collect_elliptics_remotes();

	void
	generate_groups_caches();

	void collect_info_loop_impl();
	void collect_info_loop();

	void
	cache_expire();

	void serialize();
	void deserialize();

	void cache_force_update();
	void set_update_cache_callback(const std::function<void (void)> &callback);
	void set_update_cache_ext1_callback(const std::function<void (bool)> &callback);

	void
	process_callbacks();

	std::shared_ptr<cocaine::framework::logger_t> m_logger;

	remotes_t                                          m_remotes;
	remote_t                                           m_current_remote;
	size_t                                             m_next_remote;
	std::string                                        m_cache_path;
	std::string                                        m_worker_name;

	int                                                m_metabase_timeout;
	uint64_t                                           m_metabase_current_stamp;


	synchronized_cache_map_t<namespace_state_init_t::data_t> namespaces_states;

	synchronized_cache_t<metabalancer_info_t> metabalancer_info;
	synchronized_cache_t<std::vector<namespace_settings_t>> namespaces_settings;
	synchronized_cache_t<std::map<std::string, std::vector<int>>> cache_groups;
	synchronized_cache_t<namespaces_statistics_t> namespaces_statistics;
	synchronized_cache_t<std::vector<std::string>> elliptics_remotes;

	synchronized_cache_t<std::vector<std::vector<int>>> bad_groups;
	synchronized_cache_t<std::map<int, std::vector<int>>> symmetric_groups;

	const int                                          m_group_info_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
	std::mutex                                         m_mutex;
	std::function<void (void)>                         m_cache_update_callback;
	bool                                               m_done;
	std::mutex                                         m_reconnect_mutex;

	std::chrono::seconds warning_time;
	std::chrono::seconds expire_time;

	std::chrono::milliseconds enqueue_timeout;
	std::chrono::milliseconds reconnect_timeout;

	bool cache_is_expired;
	// m_cache_update_callback with cache expiration info
	std::function<void (bool)> cache_update_ext1_callback;

	std::shared_ptr<cocaine::framework::app_service_t> m_app;
	std::shared_ptr<cocaine::framework::service_manager_t> m_service_manager;
};

template <typename T>
kora::dynamic_t
mastermind_t::data::simple_enqueue(const std::string &event, const T &chunk) {
	try {
		auto g = m_app->enqueue(event, chunk);
		g.wait_for(enqueue_timeout);

		if (g.ready() == false) {
			throw std::runtime_error("enqueue timeout");
		}

		auto chunk = g.next();
		msgpack::unpacked unpacked;
		msgpack::unpack(&unpacked, chunk.data(), chunk.size());
		auto object = unpacked.get();

		kora::dynamic_t result;

		cocaine::io::type_traits<kora::dynamic_t>::unpack(object, result);

		return result;
	} catch (const std::exception &ex) {
		throw std::runtime_error("cannot process event " + event + ": " + ex.what());
	}
}

template <typename R, typename T>
bool mastermind_t::data::simple_enqueue(const std::string &event, const T &chunk, R &result) {
	try {
		auto g = m_app->enqueue(event, chunk);
		g.wait_for(enqueue_timeout);

		if (g.ready() == false) {
			return false;
		}

		auto chunk = g.next();

		result = cocaine::framework::unpack<R>(chunk);
		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: enqueue_impl: %s", ex.what());
	}
	return false;
}

template <typename T>
kora::dynamic_t
mastermind_t::data::enqueue(const std::string &event, const T &chunk) {
	try {
		bool tried_to_reconnect = false;

		if (!m_service_manager || !m_app
				|| m_app->status() != cocaine::framework::service_status::connected) {
			COCAINE_LOG_INFO(m_logger, "libmastermind: enqueue: preconnect");
			tried_to_reconnect = true;
			reconnect();
		}

		try {
			return simple_enqueue(event, chunk);
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(m_logger
					, "libmastermind: cannot process enqueue (1st try): %s"
					, ex.what());
		}

		if (tried_to_reconnect) {
			throw std::runtime_error("reconnect is useless");
		}

		reconnect();

		try {
			return simple_enqueue(event, chunk);
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(m_logger
					, "libmastermind: cannot process enqueue (2nd try): %s"
					, ex.what());
		}

		throw std::runtime_error("bad connection");
	} catch (const std::exception &ex) {
		throw std::runtime_error(std::string("cannot process enqueue: ") + ex.what());
	}
}

template <typename R, typename T>
void mastermind_t::data::enqueue(const std::string &event, const T &chunk, R &result) {
	bool tried_to_reconnect = false;
	try {
		if (!m_service_manager || !m_app || m_app->status() != cocaine::framework::service_status::connected) {
			COCAINE_LOG_INFO(m_logger, "libmastermind: enqueue: preconnect");
			tried_to_reconnect = true;
			reconnect();
		}

		if (simple_enqueue(event, chunk, result)) {
			return;
		}

		if (tried_to_reconnect) {
			throw std::runtime_error("cannot process enqueue");
		}

		reconnect();

		if (simple_enqueue(event, chunk, result)) {
			return;
		}

		throw std::runtime_error("cannot reprocess enqueue");
	} catch (const cocaine::framework::service_error_t &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: enqueue: %s", ex.what());
		throw;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: enqueue: %s", ex.what());
		throw;
	}
}

} // namespace mastermind

#endif /* SRC__MASTERMIND_IMPL_HPP */
