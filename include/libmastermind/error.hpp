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

#ifndef INCLUDE__LIBMASTERMIND__ERROR_H
#define INCLUDE__LIBMASTERMIND__ERROR_H

#include <libmastermind/common.hpp>

#include <system_error>

namespace mastermind {

namespace libmastermind_error {

enum libmastermind_error_t {
	couple_not_found,
	not_enough_memory,
	unknown_namespace,
	invalid_groups_count,
	cache_is_expired
};

} // namespace libmastermind_error

class libmastermind_category_impl
	: public std::error_category
{
public:
	const char *name() const noexcept;
	std::string message(int ev) const;
};

const std::error_category &libmastermind_category();

std::error_code make_error_code(libmastermind_error::libmastermind_error_t e);
std::error_condition make_error_condition(libmastermind_error::libmastermind_error_t e);

class couple_not_found_error
	: public std::system_error
{
public:
	couple_not_found_error();
};

class not_enough_memory_error
	: public std::system_error
{
public:
	not_enough_memory_error();
};

class unknown_namespace_error
	: public std::system_error
{
public:
	unknown_namespace_error();
};

class invalid_groups_count_error
	: public std::system_error
{
public:
	invalid_groups_count_error();
};

class cache_is_expired_error
	: public std::system_error
{
public:
	cache_is_expired_error();
};

enum class mastermind_errc {
	  update_loop_already_started = 1
	, update_loop_already_stopped
	, unknown_feedback
};

const std::error_category &
mastermind_category();

} // namespace mastermind

namespace std {

template<>
struct is_error_code_enum<mastermind::libmastermind_error::libmastermind_error_t>
	: public true_type
{};

template<>
struct is_error_code_enum<mastermind::mastermind_errc>
	: public true_type
{};

std::error_code
make_error_code(mastermind::mastermind_errc e);

std::error_condition
make_error_condition(mastermind::mastermind_errc e);

} // namespace std

namespace mastermind {

class mastermind_error : public std::runtime_error
{
public:
	mastermind_error(std::error_code error_code_);

	const std::error_code &
	code() const;

private:
	std::error_code error_code;
};

class update_loop_already_started : public mastermind_error
{
public:
	update_loop_already_started();
};

class update_loop_already_stopped : public mastermind_error
{
public:
	update_loop_already_stopped();
};

class unknown_feedback : public mastermind_error
{
public:
	unknown_feedback(group_t couple_id_, int feedback_);

	group_t
	couple_id() const;

	int
	feedback() const;

private:
	group_t m_couple_id;
	int m_feedback;
};

} // namespace mastermind

#endif /* INCLUDE__LIBMASTERMIND__ERROR_H */
