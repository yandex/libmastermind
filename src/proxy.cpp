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

#include <sstream>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <memory>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>

#include <fcntl.h>
#include <errno.h>
#include <netdb.h>

#include <sys/socket.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <elliptics/proxy.hpp>
#include <msgpack.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/common.hpp>

#include "utils.hpp"

#include <elliptics/interface.h>

namespace {

size_t uploads_need(size_t success_copies_num, size_t replication_count) {
	switch (success_copies_num) {
	case elliptics::SUCCESS_COPIES_TYPE__ANY:
		return 1;
	case elliptics::SUCCESS_COPIES_TYPE__QUORUM:
		return (replication_count >> 1) + 1;
	case elliptics::SUCCESS_COPIES_TYPE__ALL:
		return replication_count;
	default:
		return replication_count;
	}
}

bool upload_is_good(size_t success_copies_num, size_t replication_count, size_t size) {
	switch (success_copies_num) {
	case elliptics::SUCCESS_COPIES_TYPE__ANY:
		return size >= 1;
	case elliptics::SUCCESS_COPIES_TYPE__QUORUM:
		return size >= ((replication_count >> 1) + 1);
	case elliptics::SUCCESS_COPIES_TYPE__ALL:
		return size == replication_count;
	default:
		return size >= success_copies_num;
	}
}

class write_helper_t {
public:
	typedef std::vector<elliptics::lookup_result_t> LookupResults;
	typedef std::vector<int> groups_t;

	write_helper_t(int success_copies_num, int replication_count, const groups_t desired_groups)
		: success_copies_num(success_copies_num)
		, replication_count(replication_count)
		, desired_groups(desired_groups)
	{
	}

	void update_lookup(const LookupResults &tmp, bool update_ret = true) {
		groups_t groups;
		const size_t size = tmp.size();
		groups.reserve(size);

		if (update_ret) {
			ret.clear();
			ret.reserve(size);
			ret.insert(ret.end(), tmp.begin(), tmp.end());
		}

		for (auto it = tmp.begin(), end = tmp.end(); it != end; ++it) {
			groups.push_back(it->group());
		}

		upload_groups.swap(groups);
	}

	const groups_t &get_upload_groups() const {
		return upload_groups;
	}

	bool upload_is_good() const {
		return ::upload_is_good(success_copies_num, replication_count, upload_groups.size());
	}

	bool has_incomplete_groups() const {
		return desired_groups.size() != upload_groups.size();
	}

	groups_t get_incomplete_groups() {
		groups_t incomplete_groups;
		incomplete_groups.reserve(desired_groups.size() - upload_groups.size());
		std::sort(desired_groups.begin(), desired_groups.end());
		std::sort(upload_groups.begin(), upload_groups.end());
		std::set_difference(desired_groups.begin(), desired_groups.end(),
							 upload_groups.begin(), upload_groups.end(),
							 std::back_inserter(incomplete_groups));
		return incomplete_groups;
	}

	const LookupResults &get_result() const {
		return ret;
	}

private:

	int success_copies_num;
	int replication_count;
	//
	LookupResults ret;
	groups_t desired_groups;
	groups_t upload_groups;

};

} // namespace

using namespace ioremap::elliptics;

#ifdef HAVE_METABASE
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
#endif /* HAVE_METABASE */

namespace elliptics {

enum dnet_common_embed_types {
	DNET_PROXY_EMBED_DATA		= 1,
	DNET_PROXY_EMBED_TIMESTAMP
};

struct dnet_common_embed {
	uint64_t		size;
	uint32_t		type;
	uint32_t		flags;
	uint8_t			data[0];
};

static inline void dnet_common_convert_embedded(struct dnet_common_embed *e) {
	e->size = dnet_bswap64(e->size);
	e->type = dnet_bswap32(e->type);
	e->flags = dnet_bswap32(e->flags);
}


struct mastermind_t::data {
	data(const std::string &ip, uint16_t port, int wait_timeout = 0, int group_weights_refresh_period = 60)
		: m_metabase_usage(PROXY_META_NONE)
		, m_weight_cache(get_group_weighs_cache())
		, m_group_weights_update_period(group_weights_refresh_period)
		, m_done(false)
	{
		m_service_manager = cocaine::framework::service_manager_t::create(
			cocaine::io::tcp::endpoint(ip, port));
		m_app = m_service_manager->get_service<cocaine::framework::app_service_t>("mastermind");
	}

	bool collect_group_weights();
	void collect_group_weights_loop();

	int                                                m_metabase_timeout;
	int                                                m_metabase_usage;
	uint64_t                                           m_metabase_current_stamp;

	std::unique_ptr<group_weights_cache_interface_t>   m_weight_cache;
	const int                                          m_group_weights_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
	std::mutex                                         m_mutex;
	bool                                               m_done;
	
	std::shared_ptr<cocaine::framework::service_manager_t> m_service_manager;
	std::shared_ptr<cocaine::framework::app_service_t> m_app;
};

bool mastermind_t::data::collect_group_weights() {
	if (!m_app.get()) {
		throw std::runtime_error("Mastermind is not initialized");
	}

	metabase_group_weights_request_t req;
	metabase_group_weights_response_t resp;

	req.stamp = ++m_metabase_current_stamp;

	msgpack::sbuffer buf;
	msgpack::packer<msgpack::sbuffer> pk(buf);
	pk << req;
	auto g = m_app->enqueue("get_groups_weights", std::string(buf.data(), buf.size()));
	auto chunk = g.next();
	resp = cocaine::framework::unpack<metabase_group_weights_response_t>(chunk);

	return m_weight_cache->update(resp);
}

void mastermind_t::data::collect_group_weights_loop() {
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
		tm = timeout;
		while(m_done == false)
			tm = m_weight_cache_condition_variable.wait_for(lock,
															std::chrono::seconds(
																m_group_weights_update_period));
	} while(no_timeout == tm);
}


mastermind_t::mastermind_t(const std::string &ip, uint16_t port)
	: m_data(new data(ip, port))
{
}

mastermind_t::~mastermind_t()
{
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count) {
	if(!m_data->m_weight_cache->initialized() && !m_data->collect_group_weights()) {
		return std::vector<int>();
	}
	std::vector<int> result = m_data->m_weight_cache->choose(count);

	std::ostringstream msg;

	msg << "Chosen group: [";

	std::vector<int>::const_iterator e = result.end();
	for(
		std::vector<int>::const_iterator it = result.begin();
		it != e;
		++it) {
		if(it != result.begin()) {
			msg << ", ";
		}
		msg << *it;
	}
	msg << "]\n";
	return result;
}

group_info_response_t mastermind_t::get_metabalancer_group_info(int group) {
	if (!m_data->m_app.get()) {
		throw std::runtime_error("Mastermind is not initialized");
	}

	group_info_request_t req;
	group_info_response_t resp;

	req.group = group;

	cocaine::dealer::message_path_t path("mastermind", "get_group_info");

	msgpack::sbuffer buf;
	msgpack::packer<msgpack::sbuffer> pk(buf);
	pk << req.group;

	auto g = m_data->m_app->enqueue("get_group_info", std::string(buf.data(), buf.size()));
	auto chunk = g.next();
	return cocaine::framework::unpack<group_info_response_t>(chunk);
}

std::vector<std::vector<int> > mastermind_t::get_symmetric_groups() {
	auto g = m_data->m_app->enqueue("get_symmetric_groups", "");
	auto chunk = g.next();
	return cocaine::framework::unpack<std::vector<std::vector<int>>>(chunk);
}

std::map<int, std::vector<int> > mastermind_t::get_bad_groups() {
	auto g = m_data->m_app->enqueue("get_bad_groups", "");
	auto chunk = g.next();
	return cocaine::framework::unpack<std::map<int, std::vector<int>>>(chunk);
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
		std::map<int, std::vector<int> > r2 = get_bad_groups();
		for (auto it = r2.begin(); it != r2.end(); ++it) {
			res.insert(res.end(), it->second.begin(), it->second.end());
		}
	}

	std::sort(res.begin(), res.end());
	res.erase(std::unique(res.begin(), res.end()), res.end());

	return res;
}

} // namespace elliptics
