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

#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <fstream>
#include <functional>
#include <utility>

#include <boost/lexical_cast.hpp>

#include <cocaine/traits/dynamic.hpp>
#include <cocaine/trace/trace.hpp>
#include <cocaine/framework/manager.hpp>
#include <cocaine/framework/service.hpp>
#include <cocaine/idl/node.hpp> // for app protocol spec, from node service

#include "libmastermind/mastermind.hpp"
#include "logging.hpp"
#include "utils.hpp"
#include "cache_p.hpp"
#include "namespace_state_p.hpp"
#include "cached_keys.hpp"


namespace mastermind {

struct fake_group_info_t {
	group_t id;
	groups_t groups;
	uint64_t free_effective_space;
	std::string ns;
	namespace_state_init_t::data_t::couples_t::group_info_t::status_tag group_status;
};

struct mastermind_t::data {
	data(const remotes_t &remotes, const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time_, int expire_time_, std::string worker_name,
			int enqueue_timeout_, int reconnect_timeout_,
			namespace_state_t::user_settings_factory_t user_settings_factory_, bool auto_start);
	~data();

	std::shared_ptr<namespace_state_init_t::data_t>
	get_namespace_state(const std::string &name) const;

	void
	start();

	void
	stop();

	bool
	is_running() const;

	bool
	is_valid() const;

	void reconnect();

	template <typename T>
	std::string
	simple_enqueue(const std::string &event, const T &chunk);

	// // deprecated
	// template <typename R, typename T>
	// bool simple_enqueue_old(const std::string &event, const T &chunk, R &result);

	kora::dynamic_t
	enqueue(const std::string &event);

	kora::dynamic_t
	enqueue_gzip(const std::string &event);

	kora::dynamic_t
	enqueue(const std::string &event, kora::dynamic_t args);

	template <typename T>
	std::string
	enqueue_with_reconnect(const std::string &event, const T &chunk);

	// // deprecated
	// template <typename R, typename T>
	// void enqueue_old(const std::string &event, const T &chunk, R &result);

	void
	collect_namespaces_states();

	bool collect_cached_keys();
	bool collect_elliptics_remotes();

	void collect_info_loop_impl();
	void collect_info_loop();

	void
	cache_expire();

	template <typename T>
	bool
	check_cache_for_expire(const std::string &title, const cache_t<T> &cache
			, const duration_type &preferable_life_time, const duration_type &warning_time
			, const duration_type &expire_time);

	void
	generate_fake_caches();

	void serialize();

	bool
	namespace_state_is_deleted(const kora::dynamic_t &raw_value);

	namespace_state_init_t::data_t
	create_namespaces_states(const std::string &name, const kora::dynamic_t &raw_value);

	cached_keys_t
	create_cached_keys(const std::string &name, const kora::dynamic_t &raw_value);

	std::vector<std::string>
	create_elliptics_remotes(const std::string &name, const kora::dynamic_t &raw_value);

	namespace_settings_t
	create_namespace_settings(const std::string &name, const kora::dynamic_t &raw_value);

	void deserialize();

	void
	set_user_settings_factory(namespace_state_t::user_settings_factory_t user_settings_factory_);

	void cache_force_update();
	void set_update_cache_callback(const std::function<void (void)> &callback);
	void set_update_cache_ext1_callback(const std::function<void (bool)> &callback);

	void
	process_callbacks();

	std::shared_ptr<blackhole::logger_t> m_logger;

	remotes_t                                          m_remotes;
	remote_t                                           m_current_remote;
	size_t                                             m_next_remote;
	std::string                                        m_cache_path;
	std::string                                        m_worker_name;

	int                                                m_metabase_timeout;
	uint64_t                                           m_metabase_current_stamp;


	typedef synchronized_cache_map_t<namespace_state_init_t::data_t> namespaces_states_t;
	typedef synchronized_cache_t<std::vector<std::string>> elliptics_remotes_t;

	namespaces_states_t namespaces_states;
	synchronized_cache_t<cached_keys_t> cached_keys;
	elliptics_remotes_t elliptics_remotes;

	synchronized_cache_t<std::vector<namespace_settings_t>> namespaces_settings;
	synchronized_cache_t<std::vector<groups_t>> bad_groups;
	synchronized_cache_t<std::map<group_t, fake_group_info_t>> fake_groups_info;

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

	namespace_state_t::user_settings_factory_t user_settings_factory;

	bool cache_is_expired;
	// m_cache_update_callback with cache expiration info
	std::function<void (bool)> cache_update_ext1_callback;

	std::unique_ptr<cocaine::framework::service_manager_t> m_service_manager;
	std::unique_ptr<cocaine::framework::service<cocaine::io::app_tag>> m_app;
};

namespace {

using namespace cocaine;
using namespace cocaine::framework;

struct app_request {

	typedef channel<io::app::enqueue>::sender_type sender_type;
	typedef channel<io::app::enqueue>::receiver_type receiver_type;

	typedef io::protocol<io::app::enqueue::dispatch_type>::scope::chunk chunk_verb;

	typedef task<channel<io::app::enqueue>>::future_move_type invoke_future_type;
	typedef task<sender_type>::future_move_type send_future_type;
	typedef boost::optional<std::string> result_type;
	typedef task<result_type>::future_type result_future_type;
	typedef task<result_type>::future_move_type result_future_move_type;
	typedef task<void>::future_type aggregate_future_type;

	aggregate_future_type aggregate_future;
	result_type result;

	//NOTE: 1) forced to use pointer here because receiver can't be empty constructed
	// check with cocaine guys if its feasible to add an empty constructor to receiver_type
	//NOTE: 2) and we use strong-weak pointers to communicate cancel request
	// into future chain.
	std::shared_ptr<receiver_type> rx;

	template<typename T>
	app_request(service<io::app_tag> *app, const std::string &event, const T &data) {
		aggregate_future = app->invoke<io::app::enqueue>(event)
			.then(trace_t::bind(&app_request::on_invoke<T>, this, std::placeholders::_1, data))
			;
	}

	template<typename T>
	aggregate_future_type
	on_invoke(invoke_future_type future, const T &chunk) {
		auto channel = future.get();
		auto tx = std::move(channel.tx);
		rx.reset(new receiver_type(std::move(channel.rx)));
		auto weak_rx = std::weak_ptr<receiver_type>(rx);
		return tx.send<chunk_verb>(chunk)
			.then(trace_t::bind(&app_request::on_send, this, std::placeholders::_1, weak_rx))
			.then(trace_t::bind(&app_request::on_chunk, this, std::placeholders::_1, weak_rx))
			.then(trace_t::bind(&app_request::on_choke, this, std::placeholders::_1))
			;
	}
	result_future_type
	on_send(send_future_type future, std::weak_ptr<receiver_type> weak_rx) {
		if (auto rx = weak_rx.lock()) {
			future.get();
			return rx->recv();
		} else {
			throw std::runtime_error("cancelled");
		}
	}
	result_future_type
	on_chunk(result_future_move_type future, std::weak_ptr<receiver_type> weak_rx) {
		if (auto rx = weak_rx.lock()) {
			result = std::move(future.get());
			return rx->recv();
		} else {
			throw std::runtime_error("cancelled");
		}
	}
	void
	on_choke(result_future_move_type future) {
		// to extract possible exception from the future
		future.get();
	}
};

}

template <typename T>
std::string
mastermind_t::data::simple_enqueue(const std::string &event, const T &chunk) {
	app_request request(m_app.get(), event, chunk);

	request.aggregate_future.wait_for(enqueue_timeout);

	if (!request.aggregate_future.ready()) {
		throw std::runtime_error("enqueue timeout");
	}

	if (!request.result) {
		throw std::runtime_error("absent response");
	}

	// this throws an exception on server side error
	return request.result.get();
}

template <typename T>
std::string
mastermind_t::data::enqueue_with_reconnect(const std::string &event, const T &chunk) {
	try {
		bool tried_to_reconnect = false;

		trace_t::current() = trace_t::generate(event);
		//FIXME: find a way to force blackhole to format trace attributes in hex,
		// resorted to format it manually (and only trace_id) until then
		// auto attrs = trace_t::current().attributes<blackhole::v1::attribute_list>();
		auto trace_id = trace_t::current().get_trace_id();

		if (!m_service_manager || !m_app) {
			MM_LOG_INFO(m_logger, "libmastermind: {}: preconnect", __func__);
			tried_to_reconnect = true;
			reconnect();
		}

		MM_LOG_DEBUG(m_logger, "libmastermind: {}: (1st try): sending event '{}', trace_id {:016x}", __func__, event, trace_id);
		try {
			return simple_enqueue(event, chunk);
			MM_LOG_DEBUG(m_logger, "libmastermind: {}: (1st try): received response on event '{}', trace_id {:016x}", __func__, event, trace_id);

		} catch (const std::exception &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: (1st try): error on event '{}': {}, trace_id {:016x}", __func__, event, e.what(), trace_id);
		}

		if (tried_to_reconnect) {
			throw std::runtime_error("reconnect is useless");
		}

		reconnect();

		MM_LOG_DEBUG(m_logger, "libmastermind: {}: (2st try): sending event '{}', trace_id {:016x}", __func__, event, trace_id);
		try {
			return simple_enqueue(event, chunk);
			MM_LOG_DEBUG(m_logger, "libmastermind: {}: (2st try): received response on event '{}', trace_id {:016x}", __func__, event, trace_id);

		} catch (const std::exception &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: (2st try): error on event '{}': {}, trace_id {:016x}", __func__, event, e.what(), trace_id);
		}

		throw std::runtime_error("bad connection");

	} catch (const std::exception &e) {
		MM_LOG_ERROR(m_logger, "libmastermind: {}: error on event '{}': {}", __func__, event, e.what());

		throw std::runtime_error(std::string("error on event '") + event + "': " + e.what());
	}
}

// template <typename R, typename T>
// void mastermind_t::data::enqueue_old(const std::string &event, const T &chunk, R &result) {
// 	bool tried_to_reconnect = false;
// 	try {
// 		if (!m_service_manager || !m_app) {
// 			MM_LOG_INFO(m_logger, "libmastermind: {}: preconnect", __func__);
// 			tried_to_reconnect = true;
// 			reconnect();
// 		}

// 		if (simple_enqueue_old(event, chunk, result)) {
// 			return;
// 		}

// 		if (tried_to_reconnect) {
// 			throw std::runtime_error("cannot process enqueue");
// 		}

// 		reconnect();

// 		if (simple_enqueue_old(event, chunk, result)) {
// 			return;
// 		}

// 		throw std::runtime_error("cannot reprocess enqueue");

// 	} catch (const cocaine::framework::error_t &e) {
// 		MM_LOG_ERROR(m_logger, "libmastermind: {}: {}", __func__, e.what());
// 		throw;
// 	} catch (const std::exception &e) {
// 		MM_LOG_ERROR(m_logger, "libmastermind: {}: {}", __func__, e.what());
// 		throw;
// 	}
// }

template <typename T>
bool
mastermind_t::data::check_cache_for_expire(const std::string &title, const cache_t<T> &cache
		, const duration_type &preferable_life_time, const duration_type &warning_time
		, const duration_type &expire_time) {
	bool is_expired = cache.is_expired();

	auto life_time = std::chrono::duration_cast<std::chrono::seconds>(
			clock_type::now() - cache.get_last_update_time());

	if (expire_time <= life_time) {
		MM_LOG_WARNING(m_logger, "cache \"{}\" has been expired; life-time={}s",
			title, static_cast<int>(life_time.count())
		);
		is_expired = true;
	} else if (warning_time <= life_time) {
		MM_LOG_WARNING(m_logger, "cache \"{}\" will be expired soon; life-time={}s",
			title, static_cast<int>(life_time.count())
		);
	} else if (preferable_life_time <= life_time) {
		MM_LOG_WARNING(m_logger, "cache \"{}\" is too old; life-time={}s",
			title, static_cast<int>(life_time.count())
		);
	}

	return is_expired;
}

} // namespace mastermind

#endif /* SRC__MASTERMIND_IMPL_HPP */
