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

#ifdef HAVE_METABASE
#include <cocaine/dealer/dealer.hpp>
#endif /* HAVE_METABASE */

#include <boost/shared_ptr.hpp>
#include <boost/tokenizer.hpp>

#define BOOST_PARAMETER_MAX_ARITY 10
#include <boost/parameter.hpp>

#include <elliptics/cppdef.h>

namespace elliptics {

enum metabase_type {
	PROXY_META_NONE = 0,
	PROXY_META_OPTIONAL,
	PROXY_META_NORMAL,
	PROXY_META_MANDATORY
};

#ifdef HAVE_METABASE
struct MetabaseRequest {
	int		groups_num;
	uint64_t	stamp;
	std::vector<uint8_t> id;
	MSGPACK_DEFINE(groups_num, stamp, id)
};

struct MetabaseResponse {
	std::vector<int> groups;
	uint64_t	stamp;
	MSGPACK_DEFINE(groups, stamp)
};

enum group_info_status {
	GROUP_INFO_STATUS_OK,
	GROUP_INFO_STATUS_BAD,
	GROUP_INFO_STATUS_COUPLED
};

struct GroupInfoRequest {
	int group;
	MSGPACK_DEFINE(group)
};

struct GroupInfoResponse {
	std::vector<std::string> nodes;
	std::vector<int> couples;
	int status;
};
#endif /* HAVE_METABASE */

class ID {
public:
	ID();
	ID(struct dnet_id &id);

	std::string str() const;
	std::string dump(unsigned int len = 6) const;
	struct dnet_id dnet_id() const;

	int group() const;
private:
	struct dnet_id id_;
	bool empty_;
};

class Key {
public:
	Key();
	Key(std::string filename, int column=0);
	Key(ID &id);

	bool byId() const;
	const std::string filename() const;
	const int column() const;
	struct dnet_id dnet_id() const;
	const ID id() const;
	const std::string str() const;
private:
	bool byId_;
	std::string filename_;
	int column_;
	ID id_;
};

class LookupResult {
public:
	std::string hostname;
	uint16_t port;
	std::string path;
	int group;
};

class embed {
public:
	uint32_t type;
	uint32_t flags;
	std::string data;
	virtual const std::string pack() const = 0;
};

class ReadResult {
public:
	std::string data;
	std::vector<boost::shared_ptr<embed> > embeds;
};

BOOST_PARAMETER_NAME(key)
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

class EllipticsProxy {
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

		std::string            log_path;
		uint32_t               log_mask;
		std::vector<EllipticsProxy::remote>  remotes;

		/*
		 * Specifies wether given node will join the network,
		 * or it is a client node and its ID should not be checked
		 * against collision with others.
		 *
		 * Also has a bit to forbid route list download.
		 */
		int                    flags;

		// Namespace
		std::string            ns;

		// Wait timeout in seconds used for example to wait for remote content sync.
		unsigned int           wait_timeout;

		// Wait until transaction acknowledge is received.
		long                   check_timeout;

		std::vector<int>       groups;
		int                    base_port;
		int                    directory_bit_num;
		int                    success_copies_num;
		int                    state_num;
		int                    replication_count;
		int                    chunk_size;
		bool                   eblob_style_path;

#ifdef HAVE_METABASE
		std::string            metabase_write_addr;
		std::string            metabase_read_addr;

		std::string            cocaine_config;
#endif /* HAVE_METABASE */
	};


private:
	typedef boost::char_separator<char> Separator;
	typedef boost::tokenizer<Separator> Tokenizer;

public:
	EllipticsProxy(const EllipticsProxy::config &c);
	//virtual ~EllipticsProxy();

public:
	BOOST_PARAMETER_MEMBER_FUNCTION(
		(LookupResult), lookup, tag,
		(required
			(key, (Key))
		)
		(optional
			(groups, (const std::vector<int>), std::vector<int>())
		)
	)
	{
		return lookup_impl(key, groups);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<LookupResult>), write, tag,
		(required
			(key, (Key))
			(data, (std::string))
		)
		(optional
			(offset, (uint64_t), 0)
			(size, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(replication_count, (unsigned int), 0)
			(embeds, (std::vector<boost::shared_ptr<embed> >), std::vector<boost::shared_ptr<embed> >())
		)
	)
	{
		return write_impl(key, data, offset, size, cflags, ioflags, groups, replication_count, embeds);
	}

	BOOST_PARAMETER_MEMBER_FUNCTION(
		(ReadResult), read, tag,
		(required
			(key, (Key))
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
			(key, (Key))
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
			(from, (Key))
			(to, (Key))
		)
		(optional
			(limit_start, (uint64_t), 0)
			(limit_num, (uint64_t), 0)
			(cflags, (uint64_t), 0)
			(ioflags, (uint64_t), 0)
			(groups, (const std::vector<int>), std::vector<int>())
			(key, (Key), Key())
		)
	)
	{
		return range_get_impl(from, to, cflags, ioflags, limit_start, limit_num, groups, key);
	}

#ifdef HAVE_METABASE
	BOOST_PARAMETER_MEMBER_FUNCTION(
		(std::vector<int>), get_metabalancer_groups, tag,
		(optional
			(count, (uint64_t), 0)
			(size, (uint64_t), 0)
			(key, (Key), Key())
		)
	)
	{
		return get_metabalancer_groups_impl(count, size, key);
	}

	GroupInfoResponse get_metabalancer_group_info(int group) {
		return get_metabalancer_group_info_impl(group);
	}
#endif /* HAVE_METABASE */

private:
	LookupResult lookup_impl(Key &key, std::vector<int> &groups);

	std::vector<LookupResult> write_impl(Key &key, std::string &data, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				unsigned int replication_count, std::vector<boost::shared_ptr<embed> > embeds);

	ReadResult read_impl(Key &key, uint64_t offset, uint64_t size,
				uint64_t cflags, uint64_t ioflags, std::vector<int> &groups,
				bool latest, bool embeded);

	void remove_impl(Key &key, std::vector<int> &groups);

	std::vector<std::string> range_get_impl(Key &from, Key &to, uint64_t cflags, uint64_t ioflags,
				uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, Key &key);

	std::vector<LookupResult> parse_lookup(Key &key, std::string &l);
	std::vector<int> getGroups(Key &key, const std::vector<int> &groups, int count = 0) const;

#ifdef HAVE_METABASE
	void uploadMetaInfo(const std::vector<int> &groups, const Key &key) const;
	std::vector<int> getMetaInfo(const Key &key) const;
	std::vector<int> get_metabalancer_groups_impl(uint64_t count, uint64_t size, Key &key);
	GroupInfoResponse get_metabalancer_group_info_impl(int group);
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
	std::vector<std::string>                    remotes_;
	std::vector<int>                            groups_;
*/
private:
	boost::shared_ptr<ioremap::elliptics::log_file>  elliptics_log_;
	boost::shared_ptr<ioremap::elliptics::node>      elliptics_node_;
	std::vector<int>                            groups_;

	int                                         base_port_;
	int                                         directory_bit_num_;
	int                                         success_copies_num_;
	int                                         state_num_;
	int                                         replication_count_;
	int                                         chunk_size_;
	bool                                        eblob_style_path_;

#ifdef HAVE_METABASE
	std::auto_ptr<cocaine::dealer::dealer_t>    cocaine_dealer_;
	cocaine::dealer::message_policy_t           cocaine_default_policy_;
	int                                         metabase_timeout_;
	int                                         metabase_usage_;
	uint64_t                                    metabase_current_stamp_;

	std::string                                 metabase_write_addr_;
	std::string                                 metabase_read_addr_;
#endif /* HAVE_METABASE */
};

} // namespace elliptics

#endif // _ELLIPTICS_FASTCGI_HPP_INCLUDED_

