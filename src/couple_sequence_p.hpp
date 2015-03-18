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

#ifndef LIBMASTERMIND__SRC__COUPLE_SEQUENCE_P__HPP
#define LIBMASTERMIND__SRC__COUPLE_SEQUENCE_P__HPP

#include "libmastermind/couple_sequence.hpp"
#include "couple_weights_p.hpp"

#include <iostream>

namespace mastermind {

class couple_sequence_const_iterator_t::data_t
{
public:
	data_t(ns_state::weight::weighted_couples_info_t weighted_couples_info_)
		: weighted_couples_info(std::move(weighted_couples_info_))
		, current_index(0)
	{
		try_extract_next();
	}

	void
	try_extract_next() {
		if (weighted_couples_info.empty()) {
			return;
		}

		auto total_weight = weighted_couples_info.back().weight;
		double shoot_point = double(random()) / RAND_MAX * total_weight;
		auto it = std::lower_bound(weighted_couples_info.begin()
				, weighted_couples_info.end(), uint64_t(shoot_point));

		couple_info_t couple_info;
		couple_info.id = it->couple_info.id;
		couple_info.groups = it->couple_info.groups;
		couples_info.emplace_back(couple_info);

		weighted_couples_info.erase(it);
	}

	ns_state::weight::weighted_couples_info_t weighted_couples_info;
	std::vector<couple_info_t> couples_info;
	size_t current_index;
};

class couple_sequence_t::data_t
{
public:
	data_t(ns_state::weight::weighted_couples_info_t weighted_couples_info_)
		: weighted_couples_info(std::move(weighted_couples_info_))
	{}

	ns_state::weight::weighted_couples_info_t weighted_couples_info;
};

#define INIT_CLASS(name) \
	class name##_init_t : public name##_t \
	{ \
	public: \
		typedef name##_t::data_t data_t; \
		 \
		name##_init_t(std::shared_ptr<data_t> data_) { \
			data = std::move(data_); \
		} \
	}

INIT_CLASS(couple_sequence_const_iterator);
INIT_CLASS(couple_sequence);

} // namespace mastermind

#endif /* LIBMASTERMIND__SRC__COUPLE_SEQUENCE_P__HPP */

