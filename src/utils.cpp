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

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/stream.hpp>

#include <msgpack.hpp>

#include "logging.hpp"
#include "namespace_p.hpp"
#include "utils.hpp"

namespace mastermind {

spent_time_printer_t::spent_time_printer_t(const std::string &handler_name, const std::shared_ptr<blackhole::logger_t> &logger)
	: m_handler_name(handler_name)
	, m_logger(logger)
	, m_start_time(std::chrono::system_clock::now())
{
	MM_LOG_DEBUG(m_logger, "libmastermind: handling \'{}\'", m_handler_name);
}

spent_time_printer_t::~spent_time_printer_t() {
	const auto elapsed = std::chrono::system_clock::now() - m_start_time;
	MM_LOG_INFO(m_logger, "libmastermind: time spent for \'{}\': {} milliseconds",
		m_handler_name,
		std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
	);
}

std::string
ungzip(const std::string &gzip_string) {
	namespace bi = boost::iostreams;

	bi::filtering_streambuf<bi::input> input;
	input.push(bi::gzip_decompressor());

	std::istringstream iss(gzip_string);
	input.push(iss);

	std::ostringstream oss;
	bi::copy(input, std::ostreambuf_iterator<char>(oss));
	return oss.str();
}

} // namespace mastermind

namespace msgpack {

mastermind::group_info_response_t &operator >> (object o, mastermind::group_info_response_t &v) {
	if (o.type != type::MAP) {
		throw type_error();
	}

	object_kv *p = o.via.map.ptr;
	object_kv *const pend = o.via.map.ptr + o.via.map.size;

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
