// elliptics-fastcgi - FastCGI-module component for Elliptics file storage
// Copyright (C) 2011 Leonid A. Movsesjan <lmovsesjan@yandex-team.ru>

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

#ifndef _ELLIPTICS_FASTCGI_HPP_INCLUDED_
#define _ELLIPTICS_FASTCGI_HPP_INCLUDED_

#define HAVE_METABASE 1

#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

#ifdef HAVE_METABASE
#include <cocaine/dealer/dealer.hpp>
#endif /* HAVE_METABASE */

#include <boost/shared_ptr.hpp>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>

#define BOOST_PARAMETER_MAX_ARITY 10
#include <boost/parameter.hpp>

#include <elliptics/cppdef.h>

namespace elliptics {

enum METABASE_TYPE {
	PROXY_META_NONE = 0,
	PROXY_META_OPTIONAL,
	PROXY_META_NORMAL,
	PROXY_META_MANDATORY
};

enum SUCCESS_COPIES_TYPE {
	SUCCESS_COPIES_TYPE__ANY = -1,
	SUCCESS_COPIES_TYPE__QUORUM = -2,
	SUCCESS_COPIES_TYPE__ALL = -3

};

#ifdef HAVE_METABASE

struct metabase_group_weights_request_t {
	uint64_t stamp;
	MSGPACK_DEFINE(stamp)
};

struct metabase_group_weights_response_t {
	struct GroupWithWeight {
		std::vector<int> group_ids;
		uint64_t weight;
		MSGPACK_DEFINE(group_ids, weight)
	};
	struct SizedGroups {
		uint64_t size;
		std::vector<GroupWithWeight> weighted_groups;
		MSGPACK_DEFINE(size, weighted_groups)
	};
	std::vector<SizedGroups> info;
	MSGPACK_DEFINE(info)
};

class group_weights_cache_interface_t {
public:
	virtual ~group_weights_cache_interface_t() {};

	virtual bool update(metabase_group_weights_response_t &resp) = 0;
	virtual std::vector<int> choose(uint64_t count) = 0;
	virtual bool initialized() = 0;
};

std::auto_ptr<group_weights_cache_interface_t> get_group_weighs_cache();

enum GROUP_INFO_STATUS {
	GROUP_INFO_STATUS_OK,
	GROUP_INFO_STATUS_BAD,
	GROUP_INFO_STATUS_COUPLED
};

struct group_info_request_t {
	int group;
	MSGPACK_DEFINE(group)
};

struct group_info_response_t {
	std::vector<std::string> nodes;
	std::vector<int> couples;
	int status;
};
#endif /* HAVE_METABASE */

struct dnet_id_less {
	bool operator () (const struct dnet_id &ob1, const struct dnet_id &ob2);
};

typedef ioremap::elliptics::key key_t;

class lookup_result_t {
public:
	std::string hostname;
	uint16_t port;
	std::string path;
	int group;
	int status;
	std::string addr;
	std::string short_path;
};

class embed_t {
public:
	uint32_t type;
	uint32_t flags;
	std::string data;
	virtual const std::string pack() const = 0;
};

class read_result_t {
public:
	std::string data;
	std::vector<boost::shared_ptr<embed_t> > embeds;
};

class StatusResult {
public:
	std::string addr;
	std::string id;
	float la [3];
	uint64_t vm_total;
	uint64_t vm_free;
	uint64_t vm_cached;
	uint64_t storage_size;
	uint64_t available_size;
	uint64_t files;
	uint64_t fsid;
};

template<typename R, typename A>
class async_result {
public:
	typedef ioremap::elliptics::waiter<A> waiter_t;
	typedef std::function<A()> waiter2_t;
	typedef std::function<R(const A &)> parser_t;

private:
	struct Wrap {
		Wrap(const waiter_t &waiter)
			: waiter(waiter) {
		}

		A operator () () {
			return waiter.result();
		}

	private:
		waiter_t waiter;
	};

public:

	async_result(const waiter_t &waiter, const parser_t &parser)
		: waiter(Wrap(waiter)), parser(parser)
	{}

	async_result(const waiter2_t &waiter, const parser_t &parser)
		: waiter(waiter), parser(parser)
	{}

	R get() {
		return parser(waiter());
	}

private:
	waiter2_t waiter;
	parser_t parser;
};

typedef async_result<read_result_t, ioremap::elliptics::read_result> async_read_result_t;
typedef async_result<std::vector<lookup_result_t>, ioremap::elliptics::write_result> async_write_result_t;
typedef async_result<void, std::exception_ptr> async_remove_result_t;

BOOST_PARAMETER_NAME(key)
BOOST_PARAMETER_NAME(keys)
BOOST_PARAMETER_NAME(data)

BOOST_PARAMETER_NAME(from)
BOOST_PARAMETER_NAME(to)

BOOST_PARAMETER_NAME(groups)
BOOST_PARAMETER_NAME(column)
BOOST_PARAMETER_NAME(cflags)
BOOST_PARAMETER_NAME(ioflags)
BOOST_PARAMETER_NAME(size)
BOOST_PARAMETER_NAME(offset)
BOOST_PARAMETER_NAME(latest)
BOOST_PARAMETER_NAME(count)
BOOST_PARAMETER_NAME(embeds)
BOOST_PARAMETER_NAME(embeded)
BOOST_PARAMETER_NAME(replication_count)
BOOST_PARAMETER_NAME(limit_start)
BOOST_PARAMETER_NAME(limit_num)
BOOST_PARAMETER_NAME(script)

class elliptics_proxy_t {
public:

	class remote {
	public:
		remote(const std::string &host, const int port, const int family=2);
		std::string host;
		int port;
		int family;
	};

	class config {
	public:
		config();

		std::string log_path;
		uint32_t log_mask;
		std::vector<elliptics_proxy_t::remote> remotes;

		/*
		 * Specifies wether given node will join the network,
		 * or it is a client node and its ID should not be checked
		 * against collision with others.
		 *
		 * Also has a bit to forbid route list download.
		 */
		int flags;

		// Namespace
		std::string ns;

		// Wait timeout in seconds used for example to wait for remote content sync.
		unsigned int wait_timeout;

		// Wait until transaction acknowledge is received.
		long check_timeout;

		std::vector<int> groups;
		int base_port;
		int	directory_bit_num;
		int success_copies_num;
		int	state_num;
		int replication_count;
		int	chunk_size;
		bool eblob_style_path;

#ifdef HAVE_METABASE
		std::string metabase_write_addr;
		std::string	metabase_read_addr;

		std::string cocaine_config;
		int group_weights_refresh_period;
#endif /* HAVE_METABASE */
	};


private:
	typedef boost::char_separator<char> separator_t;
	typedef boost::tokenizer<separator_t> tokenizer_t;

public:
	elliptics_proxy_t(const elliptics_proxy_t::config &c);
#ifdef HAVE_METABASE
	virtual ~elliptics_proxy_t();
#endif //HAVE_METABASE

public:
	BOOST_PARAMETER_MEMBER_FUNCTION(
		(lookup_result_t), lookup, tag,
		(required
			(key, (key_t))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return lookup_impl(key, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<lookup_result_t>), write, tag,
		(required
			(key, (key_t))
			(data, (std::string))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(replication_count, (unsigned int), 0)
			(embeds, (std::vector<boost::shared_ptr<embed_t> >), std::vector<boost::shared_ptr<embed_t> >())
		)
	)
	{
		return write_impl(key, data, offset, size, cflags, ioflags, groups, replication_count, embeds);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(read_result_t), read, tag,
		(required
			(key, (key_t))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(latest, (bool), false)
			(embeded, (bool), false)
		)
	)
	{
		return read_impl(key, offset, size, cflags, ioflags, groups, latest, embeded);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(void), remove, tag,
		(required
			(key, (key_t))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return remove_impl(key, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<std::string>), range_get, tag,
		(required
			(from, (key_t))
			(to, (key_t))
		)
		(optional
			(limit_start, (uint64_t), 0)
			(limit_num, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(key, (key_t), key_t())
		)
	)
	{
		return range_get_impl(from, to, cflags, ioflags, limit_start, limit_num, groups, key);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::map<key_t, read_result_t>), bulk_read, tag,
		(required
			(keys, (std::vector<key_t>))
		)
		(optional
			(cflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return bulk_read_impl(keys, cflags, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<elliptics_proxy_t::remote>), lookup_addr, tag,
		(required
			(key, (key_t))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return lookup_addr_impl(key, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::map<key_t, std::vector<lookup_result_t> >), bulk_write, tag,
		(required
			(keys, (std::vector<key_t>))
			(data, (std::vector<std::string>))
		)
		(optional
			(cflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(replication_count, (unsigned int), 0)
		)
	)
	{
		return bulk_write_impl(keys, data, cflags, groups, replication_count);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::string), exec_script, tag,
		(required
			(key, (key_t))
			(script, (std::string))
			(data, (std::string))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return exec_script_impl(key, data, script, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(async_read_result_t), read_async, tag,
		(required
			(key, (key_t))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(latest, (bool), false)
			(embeded, (bool), false)
		)
	)
	{
		return read_async_impl(key, offset, size, cflags, ioflags, groups, latest, embeded);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(async_write_result_t), write_async, tag,
		(required
			(key, (key_t))
			(data, (std::string))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(replication_count, (unsigned int), 0)
			(embeds, (std::vector<boost::shared_ptr<embed_t> >), std::vector<boost::shared_ptr<embed_t> >())
		)
	)
	{
		return write_async_impl(key, data, offset, size, cflags, ioflags, groups, replication_count, embeds);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(async_remove_result_t), remove_async, tag,
		(required
			(key, (key_t))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return remove_async_impl(key, groups);
	}

	bool ping();
	std::vector<StatusResult> stat_log();

	std::string id_str(const key_t &key);

#ifdef HAVE_METABASE
	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<int>), get_metabalancer_groups, tag,
		(optional
			(count, (uint64_t), 0)
			(size, (uint64_t), 0)
			(key, (key_t), key_t())
		)
	)
	{
		return get_metabalancer_groups_impl(count, size, key);
	}

	group_info_response_t get_metabalancer_group_info(int group) {
		return get_metabalancer_group_info_impl(group);
	}
#endif /* HAVE_METABASE */

private:
	lookup_result_t lookup_impl(key_t &key, std::vector<int> &groups);

	std::vector<lookup_result_t> write_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				unsigned int replication_count, std::vector<boost::shared_ptr<embed_t> > embeds);

	read_result_t read_impl(key_t &key, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				bool latest, bool embeded);

	void remove_impl(key_t &key, std::vector<int> &groups);

	std::vector<std::string> range_get_impl(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags,
				uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);

	std::map<key_t, read_result_t> bulk_read_impl(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);

		std::vector<elliptics_proxy_t::remote> lookup_addr_impl(key_t &key, std::vector<int> &groups);

	std::map<key_t, std::vector<lookup_result_t> > bulk_write_impl(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags,
															  std::vector<int> &groups, unsigned int replication_count);

	std::string exec_script_impl(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);

	async_read_result_t read_async_impl(key_t &key, uint64_t offset, uint64_t size,
									  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
									  bool latest, bool embeded);

	async_write_result_t write_async_impl(key_t &key, std::string &data, uint64_t offset, uint64_t size,
										  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
										  unsigned int replication_count, std::vector<boost::shared_ptr<embed_t> > embeds);

	async_remove_result_t remove_async_impl(key_t &key, std::vector<int> &groups);

	lookup_result_t parse_lookup(const ioremap::elliptics::lookup_result &l);
	std::vector<lookup_result_t> parse_lookup(const ioremap::elliptics::write_result &l);

	std::vector<int> get_groups(key_t &key, const std::vector<int> &groups, int count = 0) const;

#ifdef HAVE_METABASE
	void upload_meta_info(const std::vector<int> &groups, const key_t &key) const;
	std::vector<int> get_meta_info(const key_t &key) const;
	std::vector<int> get_metabalancer_groups_impl(uint64_t count, uint64_t size, key_t &key);
	group_info_response_t get_metabalancer_group_info_impl(int group);
	bool collect_group_weights();
	void collect_group_weights_loop();
#endif /* HAVE_METABASE */

/*
	void range(fastcgi::Request *request);
	void rangeDelete(fastcgi::Request *request);
	void statLog(fastcgi::Request *request);
	void upload(fastcgi::Request *request);
	void remove(fastcgi::Request *request);
	void bulkRead(fastcgi::Request *request);
	void bulkWrite(fastcgi::Request *request);
	void execScript(fastcgi::Request *request);
	void dnet_parse_numeric_id(const std::string &value, struct dnet_id &id);


	static size_t paramsNum(Tokenizer &tok);


private:
	std::vector<std::string>					remotes_;
	std::vector<int>							groups_;
*/
private:
	boost::shared_ptr<ioremap::elliptics::file_logger> m_elliptics_log;
	boost::shared_ptr<ioremap::elliptics::node> m_elliptics_node;
	std::vector<int> m_groups;

	int m_base_port;
	int m_directory_bit_num;
	int m_success_copies_num;
	int m_state_num;
	int m_replication_count;
	int m_chunk_size;
	bool m_eblob_style_path;

#ifdef HAVE_METABASE
	std::auto_ptr<cocaine::dealer::dealer_t> m_cocaine_dealer;
	cocaine::dealer::message_policy_t m_cocaine_default_policy;
	int m_metabase_timeout;
	int m_metabase_usage;
	uint64_t m_metabase_current_stamp;

	std::string m_metabase_write_addr;
	std::string m_metabase_read_addr;

	std::auto_ptr<group_weights_cache_interface_t> m_weight_cache;
	const int m_group_weights_update_period;
	boost::thread m_weight_cache_update_thread;
#endif /* HAVE_METABASE */
};

} // namespace elliptics

#endif // _ELLIPTICS_FASTCGI_HPP_INCLUDED_

