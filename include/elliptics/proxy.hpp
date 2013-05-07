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
#include <memory>

#ifdef HAVE_METABASE
#include <cocaine/dealer/dealer.hpp>
#endif /* HAVE_METABASE */

#include <boost/tokenizer.hpp>

#define BOOST_PARAMETER_MAX_ARITY 10
#include <boost/parameter.hpp>

#include <elliptics/cppdef.h>

#include "elliptics/data_container.hpp"
#include "elliptics/lookup_result.hpp"

namespace elliptics {

enum SUCCESS_COPIES_TYPE {
	SUCCESS_COPIES_TYPE__ANY = -1,
	SUCCESS_COPIES_TYPE__QUORUM = -2,
	SUCCESS_COPIES_TYPE__ALL = -3
};

#ifdef HAVE_METABASE
struct group_info_response_t {
	std::vector<std::string> nodes;
	std::vector<int> couples;
	int status;
};
#endif /* HAVE_METABASE */

typedef ioremap::elliptics::key key_t;

class embed_t {
public:
	uint32_t type;
	uint32_t flags;
	std::string data;
	virtual const std::string pack() const = 0;
};

class status_result_t {
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

struct async_read_result_t {
	typedef ioremap::elliptics::read_result_entry inner_result_entry_t;
	typedef ioremap::elliptics::async_result<inner_result_entry_t> inner_result_t;
	typedef data_container_t outer_result_t;
	typedef std::function<outer_result_t (const inner_result_entry_t &)> parser_t;

	async_read_result_t(inner_result_t &&inner_result, const parser_t &parser)
		: inner_result(std::move(inner_result))
		, parser (parser)
	{
	}

	async_read_result_t(async_read_result_t &&ob)
		: inner_result(std::move(ob.inner_result))
		, parser(std::move(ob.parser))
	{
	}

	outer_result_t get() {
		return parser(inner_result.get_one());
	}

private:
	inner_result_t inner_result;
	parser_t parser;
};

struct async_write_result_t {
	typedef ioremap::elliptics::async_write_result inner_result_t;
	typedef elliptics::lookup_result_t outer_result_t;

	async_write_result_t(inner_result_t &&inner_result, bool eblob_style_path, int base_port)
		: m_inner_result(std::move(inner_result))
		, m_eblob_style_path(eblob_style_path)
		, m_base_port(base_port)
	{
	}

	async_write_result_t(async_write_result_t &&ob)
		: m_inner_result(std::move(ob.m_inner_result))
		, m_eblob_style_path(ob.m_eblob_style_path)
		, m_base_port(ob.m_base_port)
	{
	}

	std::vector<outer_result_t> get() {
		auto v = m_inner_result.get();
		std::vector<outer_result_t> res;
		for (auto it = v.begin(); it != v.end(); ++it)
			res.push_back(lookup_result_t(*it, m_eblob_style_path, m_base_port));
		return res;
	}

	outer_result_t get_one() {
		auto l = m_inner_result.get_one();
		return lookup_result_t(l, m_eblob_style_path, m_base_port);
	}

private:
	inner_result_t m_inner_result;
	bool m_eblob_style_path;
	int m_base_port;
};

typedef ioremap::elliptics::async_remove_result async_remove_result_t;

BOOST_PARAMETER_NAME(key)
BOOST_PARAMETER_NAME(keys)
BOOST_PARAMETER_NAME(data)
BOOST_PARAMETER_NAME(entry)

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
BOOST_PARAMETER_NAME(success_copies_num)
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
		int	die_limit;
		int replication_count;
		int	chunk_size;
		bool eblob_style_path;

#ifdef HAVE_METABASE
		std::string cocaine_config;
		int group_weights_refresh_period;
#endif /* HAVE_METABASE */
	};


private:
	typedef boost::char_separator<char> separator_t;
	typedef boost::tokenizer<separator_t> tokenizer_t;

public:
	elliptics_proxy_t(const elliptics_proxy_t::config &c);
	virtual ~elliptics_proxy_t();

public:
	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::string), get_path, tag,
		(required
			(entry, (ioremap::elliptics::lookup_result_entry))
		)
	)
	{
		return get_path_impl(entry);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<std::string>), get_paths, tag,
		(required
			(entry, (std::vector<ioremap::elliptics::lookup_result_entry>))
		)
	)
	{
		return get_path_impl(entry);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(remote), get_host, tag,
		(required
			(entry, (ioremap::elliptics::lookup_result_entry))
		)
	)
	{
		return get_host_impl(entry);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<remote>), get_hosts, tag,
		(required
			(entry, (std::vector<ioremap::elliptics::lookup_result_entry>))
		)
	)
	{
		return get_host_impl(entry);
	}

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
			(data, (data_container_t))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(success_copies_num, (int), 0)
		)
	)
	{
		return write_impl(key, data, offset, size, cflags, ioflags, groups, success_copies_num/*, embeds*/);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(data_container_t), read, tag,
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
		(std::map<key_t, data_container_t>), bulk_read, tag,
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
			(data, (std::vector<data_container_t>))
		)
		(optional
			(cflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(success_copies_num, (int), 0)
		)
	)
	{
		return bulk_write_impl(keys, data, cflags, groups, success_copies_num);
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
			(data, (data_container_t))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(success_copies_num, (int), 0)
		)
	)
	{
		return write_async_impl(key, data, offset, size, cflags, ioflags, groups, success_copies_num);
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
	std::vector<status_result_t> stat_log();

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

	std::vector<std::vector<int> > get_symmetric_groups();
	std::map<int, std::vector<int> > get_bad_groups();
	std::vector<int> get_all_groups();
#endif /* HAVE_METABASE */

private:
	class impl;
	typedef std::auto_ptr<impl> impl_ptr;
	impl_ptr pimpl;

	std::string get_path_impl(const ioremap::elliptics::lookup_result_entry &l);
	std::vector<std::string> get_path_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l);

	remote get_host_impl(const ioremap::elliptics::lookup_result_entry &l);
	std::vector<remote> get_host_impl(const std::vector<ioremap::elliptics::lookup_result_entry> &l);

	lookup_result_t lookup_impl(key_t &key, std::vector<int> &groups);

	std::vector<lookup_result_t> write_impl(key_t &key, data_container_t &data, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				int success_copies_num);

	data_container_t read_impl(key_t &key, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				bool latest, bool embeded);

	void remove_impl(key_t &key, std::vector<int> &groups);

	std::vector<std::string> range_get_impl(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags,
				uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);

	std::map<key_t, data_container_t> bulk_read_impl(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);

		std::vector<elliptics_proxy_t::remote> lookup_addr_impl(key_t &key, std::vector<int> &groups);

	std::map<key_t, std::vector<lookup_result_t> > bulk_write_impl(std::vector<key_t> &keys, std::vector<data_container_t> &data, uint64_t cflags,
															  std::vector<int> &groups, int success_copies_num);

	std::string exec_script_impl(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);

	async_read_result_t read_async_impl(key_t &key, uint64_t offset, uint64_t size,
									  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
									  bool latest, bool embeded);

	async_write_result_t write_async_impl(key_t &key, data_container_t &data, uint64_t offset, uint64_t size,
										  uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
										  int success_copies_num);

	async_remove_result_t remove_async_impl(key_t &key, std::vector<int> &groups);


	//lookup_result_t parse_lookup(const ioremap::elliptics::lookup_result_entry &l);
	//std::vector<lookup_result_t> parse_lookup(const ioremap::elliptics::write_result &l);

	std::vector<int> get_groups(key_t &key, const std::vector<int> &groups, int count = 0) const;

#ifdef HAVE_METABASE
	std::vector<int> get_metabalancer_groups_impl(uint64_t count, uint64_t size, key_t &key);
	group_info_response_t get_metabalancer_group_info_impl(int group);
#endif /* HAVE_METABASE */
};

} // namespace elliptics

#endif // _ELLIPTICS_FASTCGI_HPP_INCLUDED_

