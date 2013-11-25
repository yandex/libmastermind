// libelliptics-proxy - smart proxy for Elliptics file storage
// Copyright (C) 2012 Anton Kortunov <toshik@yandex-team.ru>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include <thread>
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <type_traits>
#include <chrono>

#include "elliptics/mastermind.hpp"
#include <msgpack.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/common.hpp>

#include "utils.hpp"

namespace {

class spent_time_printer_t {
public:
	spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger)
	: m_handler_name(handler_name)
	, m_logger(logger)
	, m_beg_time(std::chrono::system_clock::now())
	{
		COCAINE_LOG_DEBUG(m_logger, "libmastermind: handling \'%s\'", m_handler_name.c_str());
	}

	~spent_time_printer_t() {
		auto end_time = std::chrono::system_clock::now();
		COCAINE_LOG_DEBUG(m_logger, "libmastermind: time spent for \'%s\': %d milliseconds"
			, m_handler_name.c_str()
			, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_beg_time).count())
			);
	}
private:
	std::string m_handler_name;
	std::shared_ptr<cocaine::framework::logger_t> &m_logger;
	std::chrono::system_clock::time_point m_beg_time;
};

} // namespace

namespace msgpack {
inline elliptics::group_info_response_t &operator >> (object o, elliptics::group_info_response_t &v) {
	if (o.type != type::MAP) {
		throw type_error();
	}

	msgpack::object_kv *p = o.via.map.ptr;
	msgpack::object_kv *const pend = o.via.map.ptr + o.via.map.size;

	for (; p < pend; ++p) {
		std::string key;

		p->key.convert(&key);

		//			if (!key.compare("nodes")) {
		//				p->val.convert(&(v.nodes));
		//			}
		if (!key.compare("couples")) {
			p->val.convert(&(v.couples));
		}
		else if (!key.compare("status")) {
			std::string status;
			p->val.convert(&status);
			if (!status.compare("bad")) {
				v.status = elliptics::GROUP_INFO_STATUS_BAD;
			} else if (!status.compare("coupled")) {
				v.status = elliptics::GROUP_INFO_STATUS_COUPLED;
			}
		} else if (!key.compare("namespace")) {
			p->val.convert(&v.name_space);
		}
	}

	return v;
}

inline elliptics::metabase_group_weights_response_t &operator >> (
		object o,
		elliptics::metabase_group_weights_response_t &v) {
	if (o.type != type::MAP) {
		throw type_error();
	}

	msgpack::object_kv *p = o.via.map.ptr;
	msgpack::object_kv *const pend = o.via.map.ptr + o.via.map.size;

	for (; p < pend; ++p) {
		elliptics::metabase_group_weights_response_t::NamedGroups named_groups;

		p->key.convert(&named_groups.name);
		object o2 = p->val;

		msgpack::object_kv *p2 = o2.via.map.ptr;
		msgpack::object_kv *const p2end = o2.via.map.ptr + o2.via.map.size;

		for (; p2 < p2end; ++p2) {
			elliptics::metabase_group_weights_response_t::SizedGroups sized_groups;
			p2->key.convert(&sized_groups.size);
			p2->val.convert(&sized_groups.weighted_groups);
			named_groups.sized_groups.push_back(sized_groups);
		}

		v.info.push_back(named_groups);
	}

	return v;
}
}

namespace elliptics {

enum dnet_common_embed_types {
	DNET_PROXY_EMBED_DATA		= 1,
	DNET_PROXY_EMBED_TIMESTAMP
};

struct mastermind_t::data {
	data(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60)
		: m_logger(logger)
		, m_remotes(remotes)
		, m_next_remote(0)
		, m_weight_cache(get_group_weighs_cache())
		, m_group_info_update_period(group_info_update_period)
		, m_done(false)
	{
		m_bad_groups_cache = std::make_shared<std::vector<std::vector<int>>>();
		m_symmetric_groups_cache = std::make_shared<std::map<int, std::vector<int>>>();
		m_cache_groups = std::make_shared<std::map<std::string, std::vector<int>>>();

		try {
			reconnect();
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: %s", ex.what());
		}

		m_weight_cache_update_thread = std::thread(std::bind(&mastermind_t::data::collect_info_loop, this));
	}

	data(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60)
		: m_logger(logger)
		, m_next_remote(0)
		, m_weight_cache(get_group_weighs_cache())
		, m_group_info_update_period(group_info_update_period)
		, m_done(false)
	{
		m_bad_groups_cache = std::make_shared<std::vector<std::vector<int>>>();
		m_symmetric_groups_cache = std::make_shared<std::map<int, std::vector<int>>>();
		m_cache_groups = std::make_shared<std::map<std::string, std::vector<int>>>();

		m_remotes.emplace_back(host, port);
		try {
			reconnect();
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: %s", ex.what());
		}
		m_weight_cache_update_thread = std::thread(std::bind(&mastermind_t::data::collect_info_loop, this));
	}

	~data() {
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
	}

	void reconnect() {
		std::lock_guard<std::mutex> lock(m_reconnect_mutex);
		(void) lock;

		size_t end = m_next_remote;
		size_t index = m_next_remote;
		size_t size = m_remotes.size();

		do {
			auto &remote = m_remotes[index];
			try {
				m_service_manager = cocaine::framework::service_manager_t::create(
					cocaine::framework::service_manager_t::endpoint_t(remote.first, remote.second));

				auto g = m_service_manager->get_service_async<cocaine::framework::app_service_t>("mastermind");
				g.wait_for(std::chrono::seconds(4));
				if (g.ready() == false){
					COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: cannot get app_service");
					m_service_manager.reset();
					index = (index + 1) % size;
					continue;
				}
				m_app = g.get();

				COCAINE_LOG_INFO(m_logger, "libmastermind: reconnect: connected to %s:%d", remote.first.c_str(), static_cast<int>(remote.second));

				m_current_remote = remote;
				m_next_remote = (index + 1) % size;
				return;
			} catch (const cocaine::framework::service_error_t &ex) {
				COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: service_error: %s", ex.what());
			} catch (const std::exception &ex) {
				COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: %s", ex.what());
			}

			index = (index + 1) % size;
		} while (index != end);

		m_current_remote = remote_t();
		m_service_manager.reset();
		COCAINE_LOG_ERROR(m_logger, "libmastermind: reconnect: cannot recconect to any host");
		throw std::runtime_error("reconnect error: cannot reconnect to any host");
	}

	template <typename F, typename R, typename... Args>
	void retry(F func, R &res, Args &&... args) {
		bool already_tried = false;
		try {
			if (!m_service_manager || !m_app || m_app->status() != cocaine::framework::service_status::connected) {
				COCAINE_LOG_INFO(m_logger, "libmastermind: retry: preconnect");
				already_tried = true;
				reconnect();
			}
			auto g = (m_app.get()->*func)(std::forward<Args>(args)...);
			g.wait_for(std::chrono::seconds(4));
			if (g.ready() == false) {
				if (already_tried) {
					throw std::runtime_error("cannot process enqueue");
				}
				COCAINE_LOG_INFO(m_logger, "libmastermind: retry: try to reconnect");
				reconnect();
				auto g = (m_app.get()->*func)(std::forward<Args>(args)...);
				g.wait_for(std::chrono::seconds(4));
				if (g.ready() == false) {
					throw std::runtime_error("cannot reprocess enqueue");
				}
				auto chunk = g.next();
				res = cocaine::framework::unpack<R>(chunk);
			}
			auto chunk = g.next();
			res = cocaine::framework::unpack<R>(chunk);
		} catch (const cocaine::framework::service_error_t &ex) {
			COCAINE_LOG_ERROR(m_logger, "libmastermind: retry: %s", ex.what());
			throw;
		} catch (const std::exception &ex) {
			COCAINE_LOG_ERROR(m_logger, "libmastermind: retry: %s", ex.what());
			throw;
		}
	}

	bool collect_group_weights();
	bool collect_bad_groups();
	bool collect_symmetric_groups();
	bool collect_cache_groups();
	void collect_info_loop();

	std::shared_ptr<cocaine::framework::logger_t> m_logger;

	remotes_t                                          m_remotes;
	remote_t                                           m_current_remote;
	size_t                                             m_next_remote;

	int                                                m_metabase_timeout;
	uint64_t                                           m_metabase_current_stamp;

	std::shared_ptr<std::vector<std::vector<int>>>     m_bad_groups_cache;
	std::recursive_mutex                               m_bad_groups_mutex;

	std::shared_ptr<std::map<int, std::vector<int>>>   m_symmetric_groups_cache;
	std::recursive_mutex                               m_symmetric_groups_mutex;

	std::shared_ptr<group_weights_cache_interface_t>   m_weight_cache;
	std::recursive_mutex                               m_weight_cache_mutex;

	const int                                          m_group_info_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
	std::mutex                                         m_mutex;
	bool                                               m_done;
	std::mutex                                         m_reconnect_mutex;

	std::shared_ptr<std::map<std::string, std::vector<int>>> m_cache_groups;
	std::recursive_mutex                               m_cache_groups_mutex;

	std::shared_ptr<cocaine::framework::app_service_t> m_app;
	std::shared_ptr<cocaine::framework::service_manager_t> m_service_manager;
};

bool mastermind_t::data::collect_group_weights() {
	try {
		auto cache = get_group_weighs_cache();
		metabase_group_weights_request_t req;
		metabase_group_weights_response_t resp;

		req.stamp = ++m_metabase_current_stamp;

		retry(&cocaine::framework::app_service_t::enqueue<decltype(req)>, resp, "get_group_weights", req);

		auto g = cache->update(resp);

		{
			std::lock_guard<std::recursive_mutex> lock(m_weight_cache_mutex);
			(void) lock;
			m_weight_cache.swap(cache);
		}

		return g;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_group_weights: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_bad_groups() {
	try {
		auto cache = std::make_shared<std::vector<std::vector<int>>>();
		retry(&cocaine::framework::app_service_t::enqueue<char [1]>, *cache, "get_bad_groups", "");

		{
			std::lock_guard<std::recursive_mutex> lock(m_bad_groups_mutex);
			(void) lock;
			m_bad_groups_cache.swap(cache);
		}

		return true;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_bad_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_symmetric_groups() {
	try {
		std::vector<std::vector<int>> sym_groups;
		retry(&cocaine::framework::app_service_t::enqueue<char [1]>, sym_groups, "get_symmetric_groups", "");

		auto cache = std::make_shared<std::map<int, std::vector<int>>>();

		for (auto it = sym_groups.begin(); it != sym_groups.end(); ++it) {
			for (auto ve = it->begin(); ve != it->end(); ++ve) {
				(*cache)[*ve] = *it;
			}
		}

		for (auto it = m_bad_groups_cache->begin(); it != m_bad_groups_cache->end(); ++it) {
			for (auto ve = it->begin(); ve != it->end(); ++ve) {
				(*cache)[*ve] = *it;
			}
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_symmetric_groups_mutex);
			(void) lock;
			m_symmetric_groups_cache.swap(cache);
		}
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_symmetric_groups: %s", ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_cache_groups() {
	try {
		std::vector<std::pair<std::string, std::vector<int>>> cache_groups;
		retry(&cocaine::framework::app_service_t::enqueue<char [1]>, cache_groups, "get_cached_keys", "");

		auto cache = std::make_shared<std::map<std::string, std::vector<int>>>();
		for (auto it = cache_groups.begin(); it != cache_groups.end(); ++it) {
			cache->insert(*it);
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_cache_groups_mutex);
			(void) lock;
			m_cache_groups.swap(cache);
		}
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: collect_cache_groups: %s", ex.what());
	}
	return false;
}

void mastermind_t::data::collect_info_loop() {
	std::unique_lock<std::mutex> lock(m_mutex);
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

		tm = timeout;
		do {
			tm = m_weight_cache_condition_variable.wait_for(lock,
															std::chrono::seconds(
																m_group_info_update_period));
		} while(tm == no_timeout && m_done == false);
	} while(m_done == false);
}


mastermind_t::mastermind_t(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period)
	: m_data(new data(remotes, logger, group_info_update_period))
{
}

mastermind_t::mastermind_t(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period)
	: m_data(new data(host, port, logger, group_info_update_period))
{
}

mastermind_t::~mastermind_t()
{
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count, const std::string &name_space) {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_weight_cache_mutex);
		(void) lock;

		if(!m_data->m_weight_cache->initialized() && !m_data->collect_group_weights()) {
			throw std::runtime_error("libmastermind cannot receive metabalancer groups");
		}
		return m_data->m_weight_cache->choose(count, name_space);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_metabalancer_groups: %s", ex.what());
		throw;
	}
}

group_info_response_t mastermind_t::get_metabalancer_group_info(int group) {
	try {
		group_info_response_t resp;

		m_data->retry(&cocaine::framework::app_service_t::enqueue<decltype(group)>, resp, "get_group_info", group);
		return resp;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_metabalancer_group_info: %s", ex.what());
		throw;
	}
}

std::map<int, std::vector<int>> mastermind_t::get_symmetric_groups() {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_symmetric_groups_mutex);
		(void) lock;

		if (m_data->m_symmetric_groups_cache->empty()) {
			m_data->collect_symmetric_groups();
		}

		return *m_data->m_symmetric_groups_cache;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_symmetric_groups: %s", ex.what());
		throw;
	}
}

std::vector<int> mastermind_t::get_symmetric_groups(int group) {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_symmetric_groups_mutex);
		(void) lock;

		if (m_data->m_symmetric_groups_cache->empty()) {
			m_data->collect_symmetric_groups();
		}

		auto it = m_data->m_symmetric_groups_cache->find(group);
		if (it == m_data->m_symmetric_groups_cache->end()) {
			return std::vector<int>();
		}
		return it->second;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_symmetric_groups(int): %s", ex.what());
		throw;
	}
}

std::vector<std::vector<int> > mastermind_t::get_bad_groups() {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_bad_groups_mutex);
		(void) lock;

		if (m_data->m_bad_groups_cache->empty()) {
			m_data->collect_bad_groups();
		}

		return *m_data->m_bad_groups_cache;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_bad_groups: %s", ex.what());
		throw;
	}
}

std::vector<int> mastermind_t::get_all_groups() {
	std::vector<int> res;

	auto groups = get_symmetric_groups();
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		res.push_back(it->first);
	}

	return res;
}

std::vector<int> mastermind_t::get_cache_groups(const std::string &key) {
	std::lock_guard<std::recursive_mutex> lock(m_data->m_cache_groups_mutex);
	(void) lock;

	try {
		if (m_data->m_cache_groups->empty()) {
			m_data->collect_cache_groups();
		}

		auto it = m_data->m_cache_groups->find(key);
		if (it != m_data->m_cache_groups->end())
			return it->second;
		return std::vector<int>();
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_cache_groups: %s", ex.what());
		throw;
	}
}

std::string mastermind_t::json_group_weights() {
	std::shared_ptr<group_weights_cache_interface_t> cache;
	{
		std::lock_guard<std::recursive_mutex> lock(m_data->m_weight_cache_mutex);
		(void) lock;
		cache = m_data->m_weight_cache;
	}
	return cache->to_string();
}

std::string mastermind_t::json_symmetric_groups() {
	std::shared_ptr<std::map<int, std::vector<int>>> cache;
	{
		std::lock_guard<std::recursive_mutex> lock(m_data->m_symmetric_groups_mutex);
		(void) lock;
		cache = m_data->m_symmetric_groups_cache;
	}

	std::ostringstream oss;
	oss << "{" << std::endl;
	for (auto it = cache->begin(), ite = --cache->end(); it != cache->end(); ++it) {
		oss << "\t\"" << it->first << "\" : [";
		for (auto it2b = it->second.begin(), it2 = it2b; it2 != it->second.end(); ++it2) {
			if (it2 != it2b) {
				oss << ", ";
			}
			oss << *it2;
		}
		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

std::string mastermind_t::json_bad_groups() {
	std::shared_ptr<std::vector<std::vector<int>>> cache;
	{
		std::lock_guard<std::recursive_mutex> lock(m_data->m_bad_groups_mutex);
		(void) lock;
		cache = m_data->m_bad_groups_cache;
	}

	std::ostringstream oss;
	oss << "{" << std::endl;
	for (auto it = cache->begin(), ite = --cache->end(); it != cache->end(); ++it) {
		oss << "\t[";
		for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
			if (it2 != it->begin()) {
				oss << ", ";
			}
			oss << *it2;
		}
		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

std::string mastermind_t::json_cache_groups() {
	std::shared_ptr<std::map<std::string, std::vector<int>>> cache;
	{
		std::lock_guard<std::recursive_mutex> lock(m_data->m_cache_groups_mutex);
		(void) lock;
		cache = m_data->m_cache_groups;
	}

	std::ostringstream oss;
	oss << "{" << std::endl;
	for (auto it = cache->begin(), ite = --cache->end(); it != cache->end(); ++it) {
		oss << "\t\"" << it->first << "\" : [";
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
			if (it2 != it->second.begin()) {
				oss << ", ";
			}
			oss << *it2;
		}

		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

} // namespace elliptics
