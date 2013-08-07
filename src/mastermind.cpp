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

#include "elliptics/mastermind.hpp"
#include <msgpack.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/common.hpp>

#include "utils.hpp"

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
		elliptics::metabase_group_weights_response_t::SizedGroups sized_groups;
		p->key.convert(&sized_groups.size);
		p->val.convert(&sized_groups.weighted_groups);
		v.info.push_back(sized_groups);
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
	data(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60)
		: m_logger(logger)
		, m_weight_cache(get_group_weighs_cache())
		, m_group_info_update_period(group_info_update_period)
		, m_done(false)
	{
		m_service_manager = cocaine::framework::service_manager_t::create(
			cocaine::framework::service_manager_t::endpoint_t(host, port));
		m_app = m_service_manager->get_service_async<cocaine::framework::app_service_t>("mastermind");
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

	bool collect_group_weights();
	bool collect_symmetric_groups();
	void collect_info_loop();

	std::shared_ptr<cocaine::framework::logger_t> m_logger;

	int                                                m_metabase_timeout;
	uint64_t                                           m_metabase_current_stamp;

	std::map<int, std::vector<int>>                    m_symmetric_groups_cache;
	std::mutex                                         m_symmetric_groups_mutex;

	std::unique_ptr<group_weights_cache_interface_t>   m_weight_cache;
	const int                                          m_group_info_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
	std::mutex                                         m_mutex;
	bool                                               m_done;

	std::shared_ptr<cocaine::framework::app_service_t> m_app;
	std::shared_ptr<cocaine::framework::service_manager_t> m_service_manager;
};

bool mastermind_t::data::collect_group_weights() {
	try {
		metabase_group_weights_request_t req;
		metabase_group_weights_response_t resp;

		req.stamp = ++m_metabase_current_stamp;

		msgpack::sbuffer buf;
		msgpack::packer<msgpack::sbuffer> pk(buf);
		pk << req;
		auto g = m_app->enqueue("get_group_weights", std::string(buf.data(), buf.size()));
		auto chunk = g.next();

		resp = cocaine::framework::unpack<metabase_group_weights_response_t>(chunk);

		return m_weight_cache->update(resp);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, ex.what());
	}
	return false;
}

bool mastermind_t::data::collect_symmetric_groups() {
	try {
		auto g = m_app->enqueue("get_symmetric_groups", "");
		auto chunk = g.next();
		auto sym_groups = cocaine::framework::unpack<std::vector<std::vector<int>>>(chunk);

		std::map<int, std::vector<int>> cache;
		for (auto it = sym_groups.begin(); it != sym_groups.end(); ++it) {
			auto b = it->begin();
			auto e = it->end();
			auto m = std::min_element(b, e);
			cache.insert(std::make_pair(*m, *it));
		}

		{
			std::lock_guard<std::mutex> lock(m_symmetric_groups_mutex);
			(void) lock;
			m_symmetric_groups_cache.swap(cache);
		}
		return true;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, ex.what());
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
	bool no_timeout = false;
	bool timeout = true;
	bool tm = timeout;
#endif
	do {
		collect_group_weights();
		collect_symmetric_groups();
		tm = timeout;
		while(m_done == false)
			tm = m_weight_cache_condition_variable.wait_for(lock,
															std::chrono::seconds(
																m_group_info_update_period));
	} while(no_timeout == tm);
}


mastermind_t::mastermind_t(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period)
	: m_data(new data(host, port, logger, group_info_update_period))
{
}

mastermind_t::~mastermind_t()
{
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count) {
	try {
		if(!m_data->m_weight_cache->initialized() && !m_data->collect_group_weights()) {
			return std::vector<int>();
		}
		return m_data->m_weight_cache->choose(count);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, ex.what());
		throw;
	}
}

group_info_response_t mastermind_t::get_metabalancer_group_info(int group) {
	try {
		group_info_request_t req;
		group_info_response_t resp;

		req.group = group;

		msgpack::sbuffer buf;
		msgpack::packer<msgpack::sbuffer> pk(buf);
		pk << req.group;

		auto g = m_data->m_app->enqueue("get_group_info", std::string(buf.data(), buf.size()));
		auto chunk = g.next();
		return cocaine::framework::unpack<group_info_response_t>(chunk);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, ex.what());
		throw;
	}
}

std::vector<std::vector<int> > mastermind_t::get_symmetric_groups() {
	try {
		auto g = m_data->m_app->enqueue("get_symmetric_groups", "");
		auto chunk = g.next();
		return cocaine::framework::unpack<std::vector<std::vector<int>>>(chunk);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, ex.what());
		throw;
	}
}

std::vector<int> mastermind_t::get_symmetric_groups(int group) {
	try {
		auto it = m_data->m_symmetric_groups_cache.find(group);
		if (it == m_data->m_symmetric_groups_cache.end() && !m_data->collect_symmetric_groups()) {
			return std::vector<int>();
		}
		return it->second;
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, ex.what());
		throw;
	}
}

std::vector<std::vector<int> > mastermind_t::get_bad_groups() {
	try {
		auto g = m_data->m_app->enqueue("get_bad_groups", "");
		auto chunk = g.next();
		return cocaine::framework::unpack<std::vector<std::vector<int>>>(chunk);
	} catch(const std::exception &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, ex.what());
		throw;
	}
}

std::vector<int> mastermind_t::get_all_groups() {
	std::vector<int> res;

	{
		std::vector<std::vector<int> > r1 = get_symmetric_groups();
		for (auto it = r1.begin(); it != r1.end(); ++it) {
			res.insert(res.end(), it->begin(), it->end());
		}
	}

	{
		std::vector<std::vector<int> > r2 = get_bad_groups();
		for (auto it = r2.begin(); it != r2.end(); ++it) {
			res.insert(res.end(), it->begin(), it->end());
		}
	}

	std::sort(res.begin(), res.end());
	res.erase(std::unique(res.begin(), res.end()), res.end());

	return res;
}

} // namespace elliptics
