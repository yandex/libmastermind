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

namespace std {

std::error_code
make_error_code(mastermind::mastermind_errc e) {
	return std::error_code(static_cast<int>(e), mastermind::mastermind_category());
}

std::error_condition
make_error_condition(mastermind::mastermind_errc e) {
	return std::error_condition(static_cast<int>(e), mastermind::mastermind_category());
}

} // namespace std

namespace mastermind {

const char *libmastermind_category_impl::name() const noexcept {
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

class mastermind_category_t
	: public std::error_category
{
public:
	const char *name() const noexcept {
		return "mastermind category";
	}

	std::string
	message(int ev) const {
		switch(static_cast<mastermind_errc>(ev)) {
		case mastermind_errc::update_loop_already_started:
			return "update loop already started";
		case mastermind_errc::update_loop_already_stopped:
			return "update loop already stopped";
		case mastermind_errc::unknown_feedback:
			return "unknown feedback";
		case mastermind_errc::namespace_state_not_found:
			return "namespace state not found";
		case mastermind_errc::unknown_group:
			return "unknown group";
		case mastermind_errc::remotes_empty:
			return "remotes list is empty";
		default:
			return "unknown mastermind error";
		}
	}
};

const std::error_category &
mastermind_category() {
	const static mastermind_category_t instance;
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

mastermind_error::mastermind_error(std::error_code error_code_)
	: std::runtime_error(error_code_.message())
	, error_code(std::move(error_code_))
{}

const std::error_code &
mastermind_error::code() const {
	return error_code;
}

update_loop_already_started::update_loop_already_started()
	: mastermind_error(std::make_error_code(
				mastermind::mastermind_errc::update_loop_already_started))
{}

update_loop_already_stopped::update_loop_already_stopped()
	: mastermind_error(std::make_error_code(
				mastermind::mastermind_errc::update_loop_already_stopped))
{}

unknown_feedback::unknown_feedback(group_t couple_id_, int feedback_)
	: mastermind_error(std::make_error_code(
				mastermind::mastermind_errc::unknown_feedback))
	, m_couple_id(couple_id_)
	, m_feedback(feedback_)
{}

group_t
unknown_feedback::couple_id() const {
	return m_couple_id;
}

int
unknown_feedback::feedback() const {
	return m_feedback;
}

namespace_state_not_found_error::namespace_state_not_found_error()
	: mastermind_error(std::make_error_code(
				mastermind::mastermind_errc::namespace_state_not_found))
{}

unknown_group_error::unknown_group_error(int group_)
	: mastermind_error(std::make_error_code(mastermind::mastermind_errc::unknown_group))
	, m_group(group_)
{}

int
unknown_group_error::group() const {
	return m_group;
}

remotes_empty_error::remotes_empty_error()
	: mastermind_error(std::make_error_code(mastermind::mastermind_errc::remotes_empty))
{}

} // namespace mastermind

