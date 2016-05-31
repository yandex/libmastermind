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

#ifndef SRC__NAMESPACE_P__HPP
#define SRC__NAMESPACE_P__HPP

#include "mastermind-cache/mastermind.hpp"

namespace mastermind {

struct namespace_settings_t::data {

	data();
	data(const data &d);
	data(data &&d);

	std::string name;
	int groups_count;
	std::string success_copies_num;
	std::string auth_key;
	std::vector<int> static_couple;

	std::string auth_key_for_write;
	std::string auth_key_for_read;

	std::string sign_token;
	std::string sign_path_prefix;
	std::string sign_port;

	int redirect_expire_time;
	int64_t redirect_content_length_threshold;

	bool is_active;

	bool can_choose_couple_to_upload;
	int64_t multipart_content_length_threshold;
};

} // mastermind

#endif /* SRC__NAMESPACE_P__HPP */

