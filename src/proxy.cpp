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
#include <cocaine/dealer/utils/error.hpp>
#include <msgpack.hpp>

#include "utils.hpp"

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
		return size == success_copies_num;
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
			ret.reserve(ret.size() + size);
			ret.insert(ret.end(), tmp.begin(), tmp.end());
		}

		for (auto it = tmp.begin(), end = tmp.end(); it != end; ++it) {
			groups.push_back(it->group);
		}

		upload_groups.swap(groups);
	}

	void fix_size(size_t size) {
		std::string str_size = boost::lexical_cast<std::string>(size);
		for (auto it = ret.begin(), end = ret.end(); it != end; ++it) {
			std::string &path = it->path;
			path.replace(path.begin() + path.rfind(':') + 1, path.end(), str_size);
		}
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

class elliptics_proxy_t::impl {
public:
	impl(const elliptics_proxy_t::config &c);
	~impl();

	std::string get_path_impl(const ioremap::elliptics::lookup_result_entry &l);
	std::vector<std::string> get_path_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l);

	elliptics_proxy_t::remote get_host_impl(const ioremap::elliptics::lookup_result_entry &l);
	std::vector<elliptics_proxy_t::remote> get_host_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l);

	lookup_result_t lookup_impl(key_t &key, std::vector<int> &groups);

	std::vector<lookup_result_t> write_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);

	read_result_t read_impl(key_t &key, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				bool latest, bool embeded);

	void remove_impl(key_t &key, std::vector<int> &groups);

	std::vector<std::string> range_get_impl(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags,
				uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);

	std::map<key_t, read_result_t> bulk_read_impl(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);

		std::vector<elliptics_proxy_t::remote> lookup_addr_impl(key_t &key, std::vector<int> &groups);

	std::map<key_t, std::vector<lookup_result_t> > bulk_write_impl(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags,
															  std::vector<int> &groups, unsigned int success_copies_num);

	std::string exec_script_impl(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);

	async_read_result read_async_impl(key_t &key, uint64_t offset, uint64_t size,
									  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
									  bool latest, bool embeded);

	async_write_result write_async_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
										  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
										  unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);

	async_remove_result remove_async_impl(key_t &key, std::vector<int> &groups);

	bool ping();
	std::vector<status_result_t> stat_log();

	std::string id_str(const key_t &key);

	lookup_result_t parse_lookup(const ioremap::elliptics::lookup_result_entry &l);
	std::vector<lookup_result_t> parse_lookup(const std::vector<ioremap::elliptics::lookup_result_entry> &l);

	std::vector<int> get_groups(key_t &key, const std::vector<int> &groups, int count = 0) const;

#ifdef HAVE_METABASE
	std::vector<int> get_metabalancer_groups_impl(uint64_t count, uint64_t size, key_t &key);
	group_info_response_t get_metabalancer_group_info_impl(int group);
	bool collect_group_weights();
	void collect_group_weights_loop();
#endif /* HAVE_METABASE */

private:
	std::shared_ptr<ioremap::elliptics::file_logger>   m_elliptics_log;
	std::shared_ptr<ioremap::elliptics::node>          m_elliptics_node;
	std::vector<int>                                   m_groups;

	int                                                m_base_port;
	int                                                m_directory_bit_num;
	int                                                m_success_copies_num;
	int                                                m_die_limit;
	int                                                m_replication_count;
	int                                                m_chunk_size;
	bool                                               m_eblob_style_path;

#ifdef HAVE_METABASE
	std::unique_ptr<cocaine::dealer::dealer_t>         m_cocaine_dealer;
	cocaine::dealer::message_policy_t                  m_cocaine_default_policy;
	int                                                m_metabase_timeout;
	int                                                m_metabase_usage;
	uint64_t                                           m_metabase_current_stamp;

	std::unique_ptr<group_weights_cache_interface_t>   m_weight_cache;
	const int                                          m_group_weights_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
#endif /* HAVE_METABASE */
};

// elliptics_proxy_t

elliptics_proxy_t::elliptics_proxy_t(const elliptics_proxy_t::config &c)
	: pimpl (new elliptics_proxy_t::impl(c)) {
}

std::string elliptics_proxy_t::get_path_impl(const ioremap::elliptics::lookup_result_entry &l) {
	return pimpl->get_path_impl(l);
}

std::vector<std::string> elliptics_proxy_t::get_path_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l) {
	return pimpl->get_path_impl(l);
}

elliptics_proxy_t::remote elliptics_proxy_t::get_host_impl(const ioremap::elliptics::lookup_result_entry &l) {
	return pimpl->get_host_impl(l);
}

std::vector<elliptics_proxy_t::remote> elliptics_proxy_t::get_host_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l) {
	return pimpl->get_host_impl(l);
}

lookup_result_t elliptics_proxy_t::lookup_impl(key_t &key, std::vector<int> &groups) {
	return pimpl->lookup_impl(key, groups);
}

std::vector<lookup_result_t> elliptics_proxy_t::write_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
			uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
			unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds) {
	return pimpl->write_impl(key, data, offset, size, cflags, ioflags, groups, success_copies_num, embeds);
}

read_result_t elliptics_proxy_t::read_impl(key_t &key, uint64_t offset, uint64_t size,
			uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
			bool latest, bool embeded) {
	return pimpl->read_impl(key, offset, size, cflags, ioflags, groups, latest, embeded);
}

void elliptics_proxy_t::remove_impl(key_t &key, std::vector<int> &groups) {
	pimpl->remove_impl(key, groups);
}

std::vector<std::string> elliptics_proxy_t::range_get_impl(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags,
			uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key) {
	return pimpl->range_get_impl(from, to, cflags, ioflags, limit_start, limit_num, groups, key);
}

std::map<key_t, read_result_t> elliptics_proxy_t::bulk_read_impl(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups) {
	return pimpl->bulk_read_impl(keys, cflags, groups);
}

std::vector<elliptics_proxy_t::remote> elliptics_proxy_t::lookup_addr_impl(key_t &key, std::vector<int> &groups) {
	return pimpl->lookup_addr_impl(key, groups);
}

std::map<key_t, std::vector<lookup_result_t> > elliptics_proxy_t::bulk_write_impl(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags,
														  std::vector<int> &groups, unsigned int success_copies_num) {
	return pimpl->bulk_write_impl(keys, data, cflags, groups, success_copies_num);
}

std::string elliptics_proxy_t::exec_script_impl(key_t &key, std::string &data, std::string &script, std::vector<int> &groups) {
	return pimpl->exec_script_impl(key, data, script, groups);
}

async_read_result elliptics_proxy_t::read_async_impl(key_t &key, uint64_t offset, uint64_t size,
								  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
								  bool latest, bool embeded) {
	return pimpl->read_async_impl(key, offset, size, cflags, ioflags, groups, latest, embeded);
}

async_write_result elliptics_proxy_t::write_async_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
									  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
									  unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds) {
	return pimpl->write_async_impl(key, data, offset, size, cflags, ioflags, groups, success_copies_num, embeds);
}

async_remove_result elliptics_proxy_t::remove_async_impl(key_t &key, std::vector<int> &groups) {
	return pimpl->remove_async_impl(key, groups);
}

std::vector<int> elliptics_proxy_t::get_groups(key_t &key, const std::vector<int> &groups, int count) const {
	return pimpl->get_groups(key, groups, count);
}

#ifdef HAVE_METABASE

std::vector<int> elliptics_proxy_t::get_metabalancer_groups_impl(uint64_t count, uint64_t size, key_t &key) {
	return pimpl->get_metabalancer_groups_impl(count, size, key);
}

group_info_response_t elliptics_proxy_t::get_metabalancer_group_info_impl(int group) {
	return pimpl->get_metabalancer_group_info_impl(group);
}

#endif /* HAVE_METABASE */

bool elliptics_proxy_t::ping() {
	return pimpl->ping();
}

std::vector<status_result_t> elliptics_proxy_t::stat_log() {
	return pimpl->stat_log();
}

std::string elliptics_proxy_t::id_str(const key_t &key) {
	return pimpl->id_str(key);
}

// pimpl

elliptics::elliptics_proxy_t::impl::impl(const elliptics_proxy_t::config &c) :
	m_groups(c.groups),
	m_base_port(c.base_port),
	m_directory_bit_num(c.directory_bit_num),
	m_success_copies_num(c.success_copies_num),
	m_die_limit(c.die_limit),
	m_replication_count(c.replication_count),
	m_chunk_size(c.chunk_size),
	m_eblob_style_path(c.eblob_style_path)
#ifdef HAVE_METABASE
	,m_cocaine_dealer()
	,m_metabase_usage(PROXY_META_NONE)
	,m_weight_cache(get_group_weighs_cache())
	,m_group_weights_update_period(c.group_weights_refresh_period)
#endif /* HAVE_METABASE */
{
	if (!c.remotes.size()) {
		throw std::runtime_error("Remotes can't be empty");
	}

	struct dnet_config dnet_conf;
	memset(&dnet_conf, 0, sizeof (dnet_conf));

	dnet_conf.wait_timeout = c.wait_timeout;
	dnet_conf.check_timeout = c.check_timeout;
	dnet_conf.flags = c.flags;

	m_elliptics_log.reset(new ioremap::elliptics::file_logger(c.log_path.c_str(), c.log_mask));
	m_elliptics_node.reset(new ioremap::elliptics::node(*m_elliptics_log, dnet_conf));

	for (std::vector<elliptics_proxy_t::remote>::const_iterator it = c.remotes.begin(); it != c.remotes.end(); ++it) {
		try {
			m_elliptics_node->add_remote(it->host.c_str(), it->port, it->family);
		} catch(const std::exception &e) {
			std::stringstream msg;
			msg << "Can't connect to remote node " << it->host << ":" << it->port << ":" << it->family << " : " << e.what() << std::endl;
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		}
	}
#ifdef HAVE_METABASE
	if (c.cocaine_config.size()) {
		m_cocaine_dealer.reset(new cocaine::dealer::dealer_t(c.cocaine_config));
	}

	m_cocaine_default_policy.deadline = c.wait_timeout;
	if (m_cocaine_dealer.get()) {
		m_weight_cache_update_thread = std::thread(std::bind(&elliptics_proxy_t::impl::collect_group_weights_loop, this));
	}
#endif /* HAVE_METABASE */
}

elliptics::elliptics_proxy_t::impl::~impl() {
#ifdef HAVE_METABASE
	m_weight_cache_condition_variable.notify_one();
	m_weight_cache_update_thread.join();
#endif /* HAVE_METABASE */
}

lookup_result_t parse_lookup(const ioremap::elliptics::lookup_result_entry &l, bool eblob_style_path, int base_port)
{
	lookup_result_t result;

	struct dnet_cmd *cmd = l.command();
	struct dnet_addr *addr = l.storage_address();
	struct dnet_file_info *info = l.file_info();
	dnet_convert_file_info(info);

	char hbuf[NI_MAXHOST];
	memset(hbuf, 0, NI_MAXHOST);

	if (getnameinfo((const sockaddr*)addr, addr->addr_len, hbuf, sizeof(hbuf), NULL, 0, 0) != 0) {
		throw std::runtime_error("can not make dns lookup");
	}
	result.hostname.assign(hbuf);

	result.port = dnet_server_convert_port((struct sockaddr *)addr->addr, addr->addr_len);
	result.group = cmd->id.group_id;
	result.status = cmd->status;
	result.short_path.assign((char *)(info + 1));
	char addr_dst[512];
	dnet_server_convert_dnet_addr_raw(addr, addr_dst, sizeof (addr_dst) - 1);
	result.addr.assign(addr_dst);

	if (eblob_style_path) {
		result.path = l.file_path();
		result.path = result.path.substr(result.path.find_last_of("/\\") + 1);
		std::ostringstream oss;
		oss << '/' << (result.port - base_port) << '/'
			<< result.path << ':' << info->offset
			<< ':' << info->size;
		oss.str().swap(result.path);
	} else {
		//struct dnet_id id;
		//elliptics_node_->transform(key.filename(), id);
		//result.path = "/" + boost::lexical_cast<std::string>(port - base_port_) + '/' + hex_dir + '/' + id;
	}

	return result;
}

std::vector<lookup_result_t> parse_lookup(const std::vector<lookup_result_entry> &l, bool eblob_style_path, int base_port)
{
	std::vector<lookup_result_t> ret;

	for (size_t i = 0; i < l.size(); ++i)
		ret.push_back(parse_lookup(l[i], eblob_style_path, base_port));

	return ret;
}

lookup_result_t elliptics_proxy_t::impl::parse_lookup(const ioremap::elliptics::lookup_result_entry &l)
{
	return elliptics::parse_lookup(l, m_eblob_style_path, m_base_port);
}

std::vector<lookup_result_t> elliptics_proxy_t::impl::parse_lookup(const std::vector<lookup_result_entry> &l)
{
	return elliptics::parse_lookup(l, m_eblob_style_path, m_base_port);
}


std::string elliptics_proxy_t::impl::get_path_impl(const ioremap::elliptics::lookup_result_entry &l) {
	std::string path;
	struct dnet_addr *addr = l.storage_address();
	struct dnet_file_info *info = l.file_info();
	dnet_convert_file_info(info);
	uint16_t port = dnet_server_convert_port((struct sockaddr *)addr->addr, addr->addr_len);
	if (m_eblob_style_path) {
		path = l.file_path();
		path = path.substr(path.find_last_of("/\\") + 1);
		std::ostringstream oss;
		oss << '/' << (port - m_base_port) << '/'
			<< path << ':' << info->offset
			<< ':' << info->size;
		oss.str().swap(path);
	} else {
		//struct dnet_id id;
		//elliptics_node_->transform(key.filename(), id);
		//result.path = "/" + boost::lexical_cast<std::string>(port - base_port_) + '/' + hex_dir + '/' + id;
	}
	return path;
}

std::vector<std::string> elliptics_proxy_t::impl::get_path_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l) {
	std::vector<std::string> ret;
	for (auto it = l.begin(), end = l.end(); it != end; ++it) {
		ret.push_back(get_path_impl(*it));
	}
	return ret;
}

elliptics_proxy_t::remote elliptics_proxy_t::impl::get_host_impl(const ioremap::elliptics::lookup_result_entry &l) {
	struct dnet_addr *addr = l.storage_address();
	struct dnet_file_info *info = l.file_info();
	dnet_convert_file_info(info);

	char hbuf[NI_MAXHOST];
	memset(hbuf, 0, NI_MAXHOST);

	if (getnameinfo((const sockaddr*)addr, addr->addr_len, hbuf, sizeof(hbuf), NULL, 0, 0) != 0) {
		throw std::runtime_error("can not make dns lookup");
	}
	std::string host(hbuf);

	uint16_t port = dnet_server_convert_port((struct sockaddr *)addr->addr, addr->addr_len);
	return elliptics_proxy_t::remote(host, port);
}

std::vector<elliptics_proxy_t::remote> elliptics_proxy_t::impl::get_host_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l) {
	std::vector<elliptics_proxy_t::remote> ret;
	for (auto it = l.begin(), end = l.end(); it != end; ++it) {
		ret.push_back(get_host_impl(*it));
	}
	return ret;
}

lookup_result_t elliptics_proxy_t::impl::lookup_impl(key_t &key, std::vector<int> &groups)
{
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;
	lookup_result_t result;

	lgroups = get_groups(key, groups);

	std::vector<int> tmp_group = {1};
	for (auto it = lgroups.begin(), end = lgroups.end(); it != end; ++it) {
		tmp_group [0] = *it;
		try {
			elliptics_session.set_groups(tmp_group);
			result = parse_lookup(elliptics_session.lookup(key).get_one());
		} catch (const std::exception &e) {
			std::stringstream msg;
			msg << "can not get download info for key " << key.to_string() << " from " << *it << " group; " << e.what() << std::endl;
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
			continue;
		}
		catch (...) {
			std::stringstream msg;
			msg << "can not get download info for key " << key.to_string() << " from " << *it << " group" << std::endl;
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
			continue;
		}
		return result;
	}

	std::stringstream msg;
	msg << "can not get download info for key " << key.to_string() << "from any allowed group!";
	m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
	throw std::runtime_error(msg.str());
}

read_result_t elliptics_proxy_t::impl::read_impl(key_t &key, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				bool latest, bool embeded)
{
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;
	lgroups = get_groups(key, groups);

	elliptics_session.set_cflags(cflags);
	elliptics_session.set_ioflags(ioflags);

	ioremap::elliptics::data_pointer result;
	read_result_t ret;

	try {
		elliptics_session.set_groups(lgroups);

		if (latest)
			result = elliptics_session.read_latest(key, offset, size).get_one().file();
		else
			result = elliptics_session.read_data(key, offset, size).get_one().file();

		if (embeded) {
			while (result.size()) {
				if (result.size() < sizeof(struct dnet_common_embed)) {
					std::ostringstream str;
					str << key.to_string() << ": offset: " << result.offset() << ", size: " << result.size() << ": invalid size";
					throw std::runtime_error(str.str());
				}

				struct dnet_common_embed *e = result.data<struct dnet_common_embed>();

				dnet_common_convert_embedded(e);

				result = result.skip<struct dnet_common_embed>();

				if (result.size() < e->size + sizeof (struct dnet_common_embed)) {
					break;
				}

				if (e->type == DNET_PROXY_EMBED_DATA) {
					break;
				}

				result = result.skip(e->size);
			}
			ret.data = result.to_string();
		} else {
			ret.data = result.to_string();
		}

	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "can not get data for key " << key.to_string() << " " << e.what() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "can not get data for key " << key.to_string() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}

	return ret;
}

std::vector<lookup_result_t> elliptics_proxy_t::impl::write_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
					uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
					unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds)
{
	unsigned int replication_count = groups.size();
	session elliptics_session(*m_elliptics_node);
	bool use_metabase = false;

	elliptics_session.set_cflags(cflags);
	elliptics_session.set_ioflags(ioflags);

	if (elliptics_session.state_num() < m_die_limit) {
		throw std::runtime_error("Too low number of existing states");
	}

	if (replication_count == 0) {
		replication_count = m_replication_count;
	}

	std::vector<int> lgroups = get_groups(key, groups);
#ifdef HAVE_METABASE
	if (m_metabase_usage >= PROXY_META_OPTIONAL) {
		try {
			if (groups.size() != replication_count || m_metabase_usage == PROXY_META_MANDATORY) {
				std::vector<int> mgroups = get_metabalancer_groups_impl(replication_count, size, key);
				lgroups = mgroups;
			}
			use_metabase = 1;
		} catch (std::exception &e) {
			m_elliptics_log->log(DNET_LOG_ERROR, e.what());
			if (m_metabase_usage >= PROXY_META_NORMAL) {
				throw std::runtime_error("Metabase does not respond");
			}
		}
	}
#endif /* HAVE_METABASE */
	if (replication_count != 0 && (size_t)replication_count < lgroups.size())
		lgroups.erase(lgroups.begin() + replication_count, lgroups.end());

	write_helper_t helper(success_copies_num != 0 ? success_copies_num : m_success_copies_num, replication_count, lgroups);

	try {
		elliptics_session.set_groups(lgroups);

		bool chunked = false;

		std::string content;

		for (std::vector<std::shared_ptr<embed_t> >::const_iterator it = embeds.begin(); it != embeds.end(); it++) {
			content.append((*it)->pack());
		}
		content.append(data);

		if (m_chunk_size && content.size() > static_cast<size_t>(m_chunk_size) && !key.by_id()
				&& !(ioflags & (DNET_IO_FLAGS_PREPARE | DNET_IO_FLAGS_COMMIT | DNET_IO_FLAGS_PLAIN_WRITE))) {
			chunked = true;
		}

		std::vector<write_result_entry> lookup;

		try {
			if (ioflags & DNET_IO_FLAGS_PREPARE) {
				lookup = elliptics_session.write_prepare(key, content, offset, size).get();
			} else if (ioflags & DNET_IO_FLAGS_COMMIT) {
				lookup = elliptics_session.write_commit(key, content, offset, size).get();
			} else if (ioflags & DNET_IO_FLAGS_PLAIN_WRITE) {
				lookup = elliptics_session.write_plain(key, content, offset).get();
			} else {
				if (chunked) {
					std::string write_content;
					bool first_iter = true;
					size_t size = content.size();

					content.substr(offset, m_chunk_size).swap(write_content);
					lookup = elliptics_session.write_prepare(key, write_content, offset, content.size()).get();
					helper.update_lookup(parse_lookup(lookup), false);

					if (helper.upload_is_good()) {
						do {
							elliptics_session.set_groups(helper.get_upload_groups());
							offset += m_chunk_size;
							content.substr(offset, m_chunk_size).swap(write_content);

							if (offset + m_chunk_size >= content.length())
								lookup = elliptics_session.write_commit(key, write_content, offset, 0).get();
							else
								lookup = elliptics_session.write_plain(key, write_content, offset).get();
							helper.update_lookup(parse_lookup(lookup), first_iter);
							first_iter = false;
						} while (helper.upload_is_good() && (offset + m_chunk_size < content.length()));
					}

					helper.fix_size(size);

				} else {
					lookup = elliptics_session.write_data(key, content, offset).get();
				}
			}

			if (!chunked)
				helper.update_lookup(parse_lookup(lookup));

			if (!helper.upload_is_good()) {
				elliptics_session.set_groups(lgroups);
				elliptics_session.set_filter(ioremap::elliptics::filters::all);
				elliptics_session.remove(key.remote());
				throw std::runtime_error("Not enough copies was written, or problems with chunked upload");
			}

			if (chunked && helper.has_incomplete_groups()) {
				elliptics_session.set_groups(helper.get_incomplete_groups());
				elliptics_session.set_filter(ioremap::elliptics::filters::all);
				elliptics_session.remove(key.remote());
			}

		}
		catch (const std::exception &e) {
			std::stringstream msg;
			msg << "Can't write data for key " << key.to_string() << " " << e.what();
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
			throw;
		}
		catch (...) {
			m_elliptics_log->log(DNET_LOG_ERROR, "Can't write data for key: unknown");
			throw;
		}

		struct timespec ts;
		memset(&ts, 0, sizeof(ts));

		elliptics_session.set_cflags(0);
		elliptics_session.write_metadata(key, key.remote(), helper.get_upload_groups(), ts);
		elliptics_session.set_cflags(ioflags);
	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "Can't write data for key " << key.to_string() << " " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "Can't write data for key " << key.to_string();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}

	return helper.get_result();
}

std::vector<std::string> elliptics_proxy_t::impl::range_get_impl(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags,
					uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key)
{
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;
	lgroups = get_groups(key, groups);

	elliptics_session.set_cflags(cflags);
	elliptics_session.set_ioflags(ioflags);

	std::vector<std::string> ret;

	try {
		struct dnet_io_attr io;
		memset(&io, 0, sizeof(struct dnet_io_attr));

		if (from.by_id()) {
			memcpy(io.id, from.id().id, sizeof(io.id));
		}

		if (to.by_id()) {
			memcpy(io.parent, from.id().id, sizeof(io.parent));
		} else {
			memset(io.parent, 0xff, sizeof(io.parent));
		}

		io.start = limit_start;
		io.num = limit_num;
		io.flags = ioflags;
		io.type = from.type();


		for (size_t i = 0; i < lgroups.size(); ++i) {
			try {
				sync_read_result range_result = elliptics_session.read_data_range(io, lgroups[i]).get();

				uint64_t num = 0;

				for (size_t i = 0; i < range_result.size(); ++i) {
					read_result_entry entry = range_result[i];
					if (!(io.flags & DNET_IO_FLAGS_NODATA))
						num += entry.io_attribute()->num;
					else
						ret.push_back(entry.data().to_string());
				}

				if (io.flags & DNET_IO_FLAGS_NODATA) {
					std::ostringstream str;
					str << num;
					ret.push_back(str.str());
				}
				if (ret.size())
					break;
			} catch (...) {
				continue;
			}
		}

		if (ret.size() == 0) {
			std::ostringstream str;
			str << "READ_RANGE failed for key " << key.to_string() << " in " << groups.size() << " groups";
			throw std::runtime_error(str.str());
		}


	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "READ_RANGE failed for key " << key.to_string() << " from:" << from.to_string() << " to:" << to.to_string() << " " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "READ_RANGE failed for key " << key.to_string() << " from:" << from.to_string() << " to:" << to.to_string();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}

	return ret;
}

void elliptics_proxy_t::impl::remove_impl(key_t &key, std::vector<int> &groups)
{
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;

	lgroups = get_groups(key, groups);
	try {
		elliptics_session.set_groups(lgroups);
		std::string l;
		if (key.by_id()) {
			struct dnet_id id = key.id();
			int error = -1;

			for (size_t i = 0; i < lgroups.size(); ++i) {
				id.group_id = lgroups[i];
				try {
					elliptics_session.set_filter(ioremap::elliptics::filters::all);
					elliptics_session.remove(id);
					error = 0;
				} catch (const std::exception &e) {
					std::stringstream msg;
					msg << "Can't remove object " << key.to_string() << " in group " << groups[i] << ": " << e.what();
					m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
				}
			}

			if (error) {
				std::ostringstream str;
				str << dnet_dump_id(&id) << ": REMOVE failed";
				throw std::runtime_error(str.str());
			}
		} else {
			elliptics_session.remove(key.remote());
		}

	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "Can't remove object " << key.to_string() << " " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "Can't remove object " << key.to_string();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
}

std::map<key_t, read_result_t> elliptics_proxy_t::impl::bulk_read_impl(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups)
{
	std::map<key_t, read_result_t> ret;

	if (!keys.size())
		return ret;

	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups = get_groups(keys[0], groups);

	std::map<struct dnet_id, key_t, dnet_id_less> keys_transformed;

	try {
		elliptics_session.set_groups(lgroups);

		std::vector<struct dnet_io_attr> ios;
		ios.reserve(keys.size());

		for (std::vector<key_t>::iterator it = keys.begin(); it != keys.end(); it++) {
			struct dnet_io_attr io;
			memset(&io, 0, sizeof(io));

			key_t tmp(*it);
			if (!tmp.by_id()) {

				tmp.transform(elliptics_session);
			}


			memcpy(io.id, tmp.id().id, sizeof(io.id));
			ios.push_back(io);
			keys_transformed.insert(std::make_pair(tmp.id(), *it));
		}

		auto result = elliptics_session.bulk_read(ios).get();

		for (auto it = result.begin(), end = result.end(); it != end; ++it) {
			read_result_entry entry = *it;
			read_result_t tmp;
			tmp.data = entry.file().to_string();

			ret.insert(std::make_pair(keys_transformed[entry.command()->id], tmp));
		}


	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "can not bulk get data " << e.what() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "can not bulk get data" << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}

	return ret;
}

std::vector<elliptics_proxy_t::remote> elliptics_proxy_t::impl::lookup_addr_impl(key_t &key, std::vector<int> &groups)
{
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups = get_groups(key, groups);

	std::vector<elliptics_proxy_t::remote> addrs;

	for (std::vector<int>::const_iterator it = groups.begin();
			it != groups.end(); it++)
	{
		std::string ret;

		if (key.by_id()) {
			struct dnet_id id = key.id();
		ret = elliptics_session.lookup_address(id, *it);

		} else {
		ret = elliptics_session.lookup_address(key.remote(), *it);
		}

		size_t pos = ret.find(':');
		elliptics_proxy_t::remote addr(ret.substr(0, pos), boost::lexical_cast<int>(ret.substr(pos+1, std::string::npos)));

		addrs.push_back(addr);
	}

	return addrs;
}

std::map<key_t, std::vector<lookup_result_t> > elliptics_proxy_t::impl::bulk_write_impl(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags,
																		   std::vector<int> &groups, unsigned int success_copies_num) {
	unsigned int replication_count = groups.size();
	std::map<key_t, std::vector<lookup_result_t> > res;
	std::map<key_t, std::vector<int> > res_groups;

	if (!keys.size())
		return res;

	session elliptics_session(*m_elliptics_node);

	if (replication_count == 0)
		replication_count = m_replication_count;

	std::vector<int> lgroups = get_groups(keys[0], groups);

	std::map<struct dnet_id, key_t, dnet_id_less> keys_transformed;

	try {
		if (keys.size() != data.size())
			throw std::runtime_error("counts of keys and data are not equal");

		elliptics_session.set_groups(lgroups);

		std::vector<struct dnet_io_attr> ios;
		ios.reserve(keys.size());

		for (size_t index = 0; index != keys.size(); ++index) {
			struct dnet_io_attr io;
			memset(&io, 0, sizeof(io));

			key_t tmp(keys [index]);
			if (!tmp.by_id()) {
				tmp.transform(elliptics_session);
			}

			memcpy(io.id, tmp.id().id, sizeof(io.id));
			io.size = data[index].size();
			ios.push_back(io);
			keys_transformed.insert(std::make_pair(tmp.id(), keys [index]));
		}

		 auto result = elliptics_session.bulk_write(ios, data).get();

		 //for (size_t i = 0; i != result.size(); ++i) {
		 for (auto it = result.begin(), end = result.end(); it != end; ++it) {
			 const ioremap::elliptics::lookup_result_entry &lr = *it;//result [i];
			 lookup_result_t r = parse_lookup(lr);
			 //ID ell_id(lr->command()->id);
			 key_t key = keys_transformed [lr.command()->id];
			 res [key].push_back(r);
			 res_groups [key].push_back(r.group);
		 }

		 unsigned int replication_need =  uploads_need(success_copies_num != 0 ? success_copies_num : m_success_copies_num,
													   replication_count);

		 auto it = res_groups.begin();
		 auto end = res_groups.end();
		 for (; it != end; ++it) {
			 if (it->second.size() < replication_need)
				 break;
		 }

		 if (it != end) {
			 for (auto it = res_groups.begin(), end = res_groups.end(); it != end; ++it) {
				 elliptics_session.set_groups(it->second);
				 elliptics_session.remove(it->first.remote());
			 }
			 throw std::runtime_error("Not enough copies was written");
		 }

	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "can not bulk write data " << e.what() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "can not bulk write data" << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}

	return res;
}

std::string elliptics_proxy_t::impl::exec_script_impl(key_t &key, std::string &data, std::string &script, std::vector<int> &groups) {
	std::string res;
	ioremap::elliptics::session sess(*m_elliptics_node);
	if (sess.state_num() < m_die_limit) {
		throw std::runtime_error("Too low number of existing states");
	}

	struct dnet_id id;
	memset(&id, 0, sizeof(id));

	if (key.by_id()) {
		id = key.id();
	} else {
		sess.transform(key.remote(), id);
		id.type = key.type();
	}

	std::vector<int> lgroups = get_groups(key, groups);

	try {
		sess.set_groups(lgroups);
		res = sess.exec_locked(&id, script, data, std::string());
	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "can not execute script  " << script << "; " << e.what() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "can not execute script  " << script << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	return res;
}

async_read_result elliptics_proxy_t::impl::read_async_impl(key_t &key, uint64_t offset, uint64_t size,
												  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
												  bool latest, bool embeded) {
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;
	lgroups = get_groups(key, groups);

	elliptics_session.set_cflags(cflags);
	elliptics_session.set_ioflags(ioflags);

	try {
		elliptics_session.set_groups(lgroups);

		if (latest)
			return elliptics_session.read_latest(key, offset, size);
		else
			return elliptics_session.read_data(key, offset, size);
	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "can not get data for key " << key.to_string() << " " << e.what() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "can not get data for key " << key.to_string() << std::endl;
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
}

async_write_result elliptics_proxy_t::impl::write_async_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
													  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
													  unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds)
{
	unsigned int replication_count = groups.size();
	session elliptics_session(*m_elliptics_node);
	bool use_metabase = false;

	elliptics_session.set_cflags(cflags);
	elliptics_session.set_ioflags(ioflags);

	if (elliptics_session.state_num() < m_die_limit) {
		throw std::runtime_error("Too low number of existing states");
	}

	if (replication_count == 0) {
		replication_count = m_replication_count;
	}

	std::vector<int> lgroups = get_groups(key, groups);
#ifdef HAVE_METABASE
	if (m_metabase_usage >= PROXY_META_OPTIONAL) {
		try {
			if (groups.size() != replication_count || m_metabase_usage == PROXY_META_MANDATORY) {
				std::vector<int> mgroups = get_metabalancer_groups_impl(replication_count, size, key);
				lgroups = mgroups;
			}
			use_metabase = 1;
		} catch (std::exception &e) {
			m_elliptics_log->log(DNET_LOG_ERROR, e.what());
			if (m_metabase_usage >= PROXY_META_NORMAL) {
				throw std::runtime_error("Metabase does not respond");
			}
		}
	}
#endif /* HAVE_METABASE */
	if (replication_count != 0 && (size_t)replication_count < lgroups.size())
		lgroups.erase(lgroups.begin() + replication_count, lgroups.end());



	try {
		elliptics_session.set_groups(lgroups);

		std::string content;

		for (std::vector<std::shared_ptr<embed_t> >::const_iterator it = embeds.begin(); it != embeds.end(); it++) {
			content.append((*it)->pack());
		}
		content.append(data);

		if (ioflags & DNET_IO_FLAGS_PREPARE) {
			return elliptics_session.write_prepare(key, content, offset, size);
		} else if (ioflags & DNET_IO_FLAGS_COMMIT) {
			return elliptics_session.write_commit(key, content, offset, size);
		} else if (ioflags & DNET_IO_FLAGS_PLAIN_WRITE) {
			return elliptics_session.write_plain(key, content, offset);
		} else {
			return elliptics_session.write_data(key, content, offset);
		}
	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "Can't write data for key " << key.to_string() << " " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "Can't write data for key " << key.to_string();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
}

async_remove_result elliptics_proxy_t::impl::remove_async_impl(key_t &key, std::vector<int> &groups) {
	session elliptics_session(*m_elliptics_node);
	std::vector<int> lgroups;

	lgroups = get_groups(key, groups);
	try {
		elliptics_session.set_groups(lgroups);
		return elliptics_session.remove(key);
	}
	catch (const std::exception &e) {
		std::stringstream msg;
		msg << "Can't remove object " << key.to_string() << " " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
	catch (...) {
		std::stringstream msg;
		msg << "Can't remove object " << key.to_string();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
}

bool elliptics_proxy_t::impl::ping() {
	ioremap::elliptics::session sess(*m_elliptics_node);
	return sess.state_num() >= m_die_limit;
}

std::vector<status_result_t> elliptics_proxy_t::impl::stat_log() {
	std::vector<status_result_t> res;

	ioremap::elliptics::session sess(*m_elliptics_node);

	std::vector<ioremap::elliptics::stat_result_entry> srs = sess.stat_log().get();

	char id_str[DNET_ID_SIZE * 2 + 1];
	char addr_str[128];

	status_result_t sr;

	//for (size_t i = 0; i < srs.size(); ++i) {
	for (auto it = srs.begin(), end = srs.end(); it != end; ++it) {
		const ioremap::elliptics::stat_result_entry &data = *it;//srs[i];
		struct dnet_addr *addr = data.address();
		struct dnet_cmd *cmd = data.command();
		struct dnet_stat *st = data.statistics();

		dnet_server_convert_dnet_addr_raw(addr, addr_str, sizeof(addr_str));
		dnet_dump_id_len_raw(cmd->id.id, DNET_ID_SIZE, id_str);

		sr.la[0] = (float)st->la[0] / 100.0;
		sr.la[1] = (float)st->la[1] / 100.0;
		sr.la[2] = (float)st->la[2] / 100.0;

		sr.addr.assign(addr_str);
		sr.id.assign(id_str);
		sr.vm_total = st->vm_total;
		sr.vm_free = st->vm_free;
		sr.vm_cached = st->vm_cached;
		sr.storage_size = st->frsize * st->blocks / 1024 / 1024;
		sr.available_size = st->bavail * st->bsize / 1024 / 1024;
		sr.files = st->files;
		sr.fsid = st->fsid;

		res.push_back(sr);
	}

	return res;
}

std::string elliptics_proxy_t::impl::id_str(const key_t &key) {
	ioremap::elliptics::session sess(*m_elliptics_node);
	struct dnet_id id;
	memset(&id, 0, sizeof(id));
	if (key.by_id()) {
		id = key.id();
	} else {
		sess.transform(key.remote(), id);
	}
	char str[2 * DNET_ID_SIZE + 1];
	dnet_dump_id_len_raw(id.id, DNET_ID_SIZE, str);
	return std::string(str);
}

std::vector<int> elliptics_proxy_t::impl::get_groups(key_t &key, const std::vector<int> &groups, int count) const {
	std::vector<int> lgroups;

	if (groups.size()) {
		lgroups = groups;
	}
	else {
		lgroups = m_groups;
		if (lgroups.size() > 1) {
			std::vector<int>::iterator git = lgroups.begin();
			++git;
			std::random_shuffle(git, lgroups.end());
		}
	}

	if (count != 0 && count < (int)(lgroups.size())) {
		lgroups.erase(lgroups.begin() + count, lgroups.end());
	}

	if (!lgroups.size()) {
		throw std::runtime_error("There is no groups");
	}

	return lgroups;
}


#ifdef HAVE_METABASE
bool elliptics_proxy_t::impl::collect_group_weights()
{
	if (!m_cocaine_dealer.get()) {
		throw std::runtime_error("Dealer is not initialized");
	}

	metabase_group_weights_request_t req;
	metabase_group_weights_response_t resp;

	req.stamp = ++m_metabase_current_stamp;

	cocaine::dealer::message_path_t path("mastermind", "get_group_weights");

	boost::shared_ptr<cocaine::dealer::response_t> future;
	future = m_cocaine_dealer->send_message(req, path, m_cocaine_default_policy);

	cocaine::dealer::data_container chunk;
	future->get(&chunk);

	msgpack::unpacked unpacked;
	msgpack::unpack(&unpacked, static_cast<const char*>(chunk.data()), chunk.size());

	unpacked.get().convert(&resp);

	return m_weight_cache->update(resp);
}

void elliptics_proxy_t::impl::collect_group_weights_loop()
{
	std::mutex mutex;
	std::unique_lock<std::mutex> lock(mutex);
#if __GNUC_MINOR__ >= 6
	auto no_timeout = std::cv_status::no_timeout;
#else
	bool no_timeout = false;
#endif
	//while(!boost::this_thread::interruption_requested()) {
	while(no_timeout == m_weight_cache_condition_variable.wait_for(lock,
													 std::chrono::milliseconds(
														 m_group_weights_update_period))) {
		try {
			collect_group_weights();
			m_elliptics_log->log(DNET_LOG_INFO, "Updated group weights");
		} catch (const msgpack::unpack_error &e) {
			std::stringstream msg;
			msg << "Error while unpacking message: " << e.what();
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		} catch (const cocaine::dealer::dealer_error &e) {
			std::stringstream msg;
			msg << "Cocaine dealer error: " << e.what();
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		} catch (const cocaine::dealer::internal_error &e) {
			std::stringstream msg;
			msg << "Cocaine internal error: " << e.what();
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		} catch (const std::exception &e) {
			std::stringstream msg;
			msg << "Error while updating cache: " << e.what();
			m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		}
		//boost::this_thread::sleep(boost::posix_time::seconds(m_group_weights_update_period));
	}

}

std::vector<int> elliptics_proxy_t::impl::get_metabalancer_groups_impl(uint64_t count, uint64_t size, key_t &key)
{
	try {
		if(!m_weight_cache->initialized() && !collect_group_weights()) {
			return std::vector<int>();
		}
		std::vector<int> result = m_weight_cache->choose(count);

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
		m_elliptics_log->log(DNET_LOG_INFO, msg.str().c_str());
		return result;

	} catch (const msgpack::unpack_error &e) {
		std::stringstream msg;
		msg << "Error while unpacking message: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	} catch (const cocaine::dealer::dealer_error &e) {
		std::stringstream msg;
		msg << "Cocaine dealer error: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	} catch (const cocaine::dealer::internal_error &e) {
		std::stringstream msg;
		msg << "Cocaine internal error: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
}

group_info_response_t elliptics_proxy_t::impl::get_metabalancer_group_info_impl(int group)
{
	if (!m_cocaine_dealer.get()) {
		throw std::runtime_error("Dealer is not initialized");
	}


	group_info_request_t req;
	group_info_response_t resp;

	req.group = group;

	try {
		cocaine::dealer::message_path_t path("mastermind", "get_group_info");

		boost::shared_ptr<cocaine::dealer::response_t> future;
		future = m_cocaine_dealer->send_message(req.group, path, m_cocaine_default_policy);

		cocaine::dealer::data_container chunk;
		future->get(&chunk);

		msgpack::unpacked unpacked;
		msgpack::unpack(&unpacked, static_cast<const char*>(chunk.data()), chunk.size());

		unpacked.get().convert(&resp);

	} catch (const msgpack::unpack_error &e) {
		std::stringstream msg;
		msg << "Error while unpacking message: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	} catch (const cocaine::dealer::dealer_error &e) {
		std::stringstream msg;
		msg << "Cocaine dealer error: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	} catch (const cocaine::dealer::internal_error &e) {
		std::stringstream msg;
		msg << "Cocaine internal error: " << e.what();
		m_elliptics_log->log(DNET_LOG_ERROR, msg.str().c_str());
		throw;
	}
		

	return resp;
}
#endif /* HAVE_METABASE */

} // namespace elliptics

