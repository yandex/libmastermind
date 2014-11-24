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

#include "namespace_p.hpp"
#include "utils.hpp"
#include "libmastermind/error.hpp"

#include <cocaine/traits/tuple.hpp>

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <map>
#include <vector>
#include <sstream>

namespace mastermind {

spent_time_printer_t::spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger)
: m_handler_name(handler_name)
, m_logger(logger)
, m_beg_time(std::chrono::system_clock::now())
{
	COCAINE_LOG_DEBUG(m_logger, "libmastermind: handling \'%s\'", m_handler_name.c_str());
}

spent_time_printer_t::~spent_time_printer_t() {
	auto end_time = std::chrono::system_clock::now();
	COCAINE_LOG_INFO(m_logger, "libmastermind: time spent for \'%s\': %d milliseconds"
		, m_handler_name.c_str()
		, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_beg_time).count())
		);
}

} // namespace mastermind

namespace msgpack {
mastermind::group_info_response_t &operator >> (object o, mastermind::group_info_response_t &v) {
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
				v.status = mastermind::GROUP_INFO_STATUS_BAD;
			} else if (!status.compare("coupled")) {
				v.status = mastermind::GROUP_INFO_STATUS_COUPLED;
			}
		} else if (!key.compare("namespace")) {
			p->val.convert(&v.name_space);
		}
	}

	return v;
}

} // namespace msgpack
