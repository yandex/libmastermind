/*
	Client library for mastermind
	Copyright (C) 2013-2015 Yandex

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

#ifndef LIBMASTERMIND__INCLUDE__LIBMASTERMIND__COUPLE_SEQUENCE__HPP
#define LIBMASTERMIND__INCLUDE__LIBMASTERMIND__COUPLE_SEQUENCE__HPP

#include <libmastermind/common.hpp>

#include <memory>
#include <iterator>
#include <vector>

namespace mastermind {

class couple_info_t {
public:
	group_t id;
	groups_t groups;

protected:
};

class couple_sequence_const_iterator_t
	: public std::iterator<std::forward_iterator_tag, const couple_info_t>
{
public:
	typedef couple_sequence_const_iterator_t self_type;

	couple_sequence_const_iterator_t();
	couple_sequence_const_iterator_t(const self_type &that);

	reference
	operator * () const;

	pointer
	operator -> () const;

	bool
	operator == (const self_type &other) const;

	bool
	operator != (const self_type &other) const;

	self_type &
	operator ++ ();

	self_type
	operator ++ (int);

	self_type &
	operator = (const self_type &that);

protected:
	class data_t;

	std::shared_ptr<data_t> data;
};

class couple_sequence_t {
public:
	typedef couple_sequence_const_iterator_t const_iterator;

	const_iterator
	begin() const;

	const_iterator
	end() const;

	size_t
	size() const;

protected:
	class data_t;

	std::shared_ptr<data_t> data;

};

} // namespace mastermind

#endif /* LIBMASTERMIND__INCLUDE__LIBMASTERMIND__COUPLE_SEQUENCE__HPP */

