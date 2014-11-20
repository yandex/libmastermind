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

#include "libmastermind/error.hpp"

namespace mastermind {

const char *libmastermind_category_impl::name() const {
	return "libmastermind";
}

std::string libmastermind_category_impl::message(int ev) const {
	switch (ev) {
	case libmastermind_error::couple_not_found:
		return "Couple not found";
	case libmastermind_error::not_enough_memory:
		return "There is no couple with enough disk space";
	case libmastermind_error::unknown_namespace:
		return "Unknown namespace";
	case libmastermind_error::invalid_groups_count:
		return "Cannot find couple with such count of groups";
	case libmastermind_error::cache_is_expired:
		return "Expired cache cannot be used";
	default:
		return "Unknown libmastermind error";
	}
}

const std::error_category &libmastermind_category() {
	const static libmastermind_category_impl instance;
	return instance;
}

std::error_code make_error_code(libmastermind_error::libmastermind_error_t e) {
	return std::error_code(static_cast<int>(e), libmastermind_category());
}

std::error_condition make_error_condition(libmastermind_error::libmastermind_error_t e) {
	return std::error_condition(static_cast<int>(e), libmastermind_category());
}

couple_not_found_error::couple_not_found_error()
	: std::system_error(make_error_code(libmastermind_error::couple_not_found))
{}

not_enough_memory_error::not_enough_memory_error()
	: std::system_error(make_error_code(libmastermind_error::not_enough_memory))
{}

unknown_namespace_error::unknown_namespace_error()
	: std::system_error(make_error_code(libmastermind_error::unknown_namespace))
{}

invalid_groups_count_error::invalid_groups_count_error()
	: std::system_error(make_error_code(libmastermind_error::invalid_groups_count))
{}

cache_is_expired_error::cache_is_expired_error()
	: std::system_error(make_error_code(libmastermind_error::cache_is_expired))
{}

} // namespace mastermind

