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
#include "elliptics/async_results.hpp"

namespace elliptics {

enum SUCCESS_COPIES_TYPE {
	SUCCESS_COPIES_TYPE__ANY = -1,
	SUCCESS_COPIES_TYPE__QUORUM = -2,
	SUCCESS_COPIES_TYPE__ALL = -3
};

enum tag_user_flags {
	UF_EMBEDS = 1
};

#ifdef HAVE_METABASE
struct group_info_response_t {
	std::vector<std::string> nodes;
	std::vector<int> couples;
	int status;
};
#endif /* HAVE_METABASE */

typedef ioremap::elliptics::key key_t;

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

typedef ioremap::elliptics::find_indexes_result_entry find_indexes_result_entry_t;
typedef ioremap::elliptics::index_entry index_entry_t;
typedef ioremap::elliptics::async_update_indexes_result async_update_indexes_result_t;
typedef ioremap::elliptics::async_find_indexes_result async_find_indexes_result_t;
typedef ioremap::elliptics::async_check_indexes_result async_check_indexes_result_t;


BOOST_PARAMETER_NAME(key)
BOOST_PARAMETER_NAME(keys)
BOOST_PARAMETER_NAME(data)
BOOST_PARAMETER_NAME(indexes)

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

class mastermind_t {
public:
	mastermind_t(const std::string &ip, uint16_t port);
	~mastermind_t();

	std::vector<int> get_metabalancer_groups(uint64_t count = 0);
	group_info_response_t get_metabalancer_group_info(int group);
	std::vector<std::vector<int> > get_symmetric_groups();
	std::map<int, std::vector<int> > get_bad_groups();
	std::vector<int> get_all_groups();
private:
	struct data;
	std::unique_ptr<data> m_data;
};

} // namespace elliptics

#endif // _ELLIPTICS_FASTCGI_HPP_INCLUDED_

