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

#ifndef SRC__UTILS_HPP
#define SRC__UTILS_HPP

#include "libmastermind/mastermind.hpp"
#include "namespace_state_p.hpp"

#include <cocaine/framework/logging.hpp>

#include <string>
#include <tuple>
#include <functional>
#include <chrono>
#include <vector>
#include <ostream>

namespace mastermind {

typedef std::chrono::system_clock clock_type;
typedef clock_type::duration duration_type;
typedef clock_type::time_point time_point_type;

class spent_time_printer_t {
public:
	spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger);

	~spent_time_printer_t();

private:
	std::string m_handler_name;
	std::shared_ptr<cocaine::framework::logger_t> &m_logger;
	std::chrono::system_clock::time_point m_beg_time;
};

struct fake_group_info_t {
	group_t id;
	groups_t groups;
	uint64_t free_effective_space;
	std::string ns;
	namespace_state_init_t::data_t::couples_t::group_info_t::status_tag group_status;
};

enum GROUP_INFO_STATUS {
  GROUP_INFO_STATUS_OK,
  GROUP_INFO_STATUS_BAD,
  GROUP_INFO_STATUS_COUPLED
};

std::string
ungzip(const std::string &gzip_string);

} // namespace mastermind

template <typename T>
std::ostream &operator <<(std::ostream &ostream, const std::vector<T> &vector) {
	ostream << '[';
	{
		auto beg = vector.begin();
		auto end = vector.end();
		for (auto it = beg; it != end; ++it) {
			if (it != beg) {
				ostream << ", ";
			}

			ostream << *it;
		}
	}
	ostream << ']';

	return ostream;
}

namespace msgpack {

mastermind::group_info_response_t &operator >> (object o, mastermind::group_info_response_t &v);

} // namespace msgpack

#endif /* SRC__UTILS_HPP */
