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

#include "libmastermind/mastermind.hpp"

#include "cocaine/traits/dynamic.hpp"

#include <kora/config.hpp>

#include <tuple>
#include <map>
#include <vector>
#include <functional>
#include <mutex>

#ifndef LIBMASTERMIND__SRC__COUPLE_WEIGHTS_P__HPP
#define LIBMASTERMIND__SRC__COUPLE_WEIGHTS_P__HPP

namespace mastermind {
namespace ns_state {

namespace weight {

class couple_info_t {
public:
	couple_info_t()
		: id (-1)
		, weight(0)
		, memory(0)
		, coefficient(1)
	{}

	groups_t groups;
	group_t id;
	uint64_t weight;
	uint64_t memory;
	double coefficient;

private:
};

bool
memory_comparator(const couple_info_t &lhs, const couple_info_t &rhs);

typedef std::vector<couple_info_t> couples_info_t;

class weighted_couple_info_t {
public:
	typedef couples_info_t::const_iterator const_iterator_t;

	weighted_couple_info_t(uint64_t weight_, const_iterator_t couple_info_)
		: weight(weight_)
		, couple_info(std::move(couple_info_))
	{}

	uint64_t weight;
	const_iterator_t couple_info;

	friend
	bool
	operator < (const weighted_couple_info_t &lhs, const weighted_couple_info_t &rhs) {
		return lhs.weight < rhs.weight;
	}

	friend
	bool
	operator < (const weighted_couple_info_t &lhs, const uint64_t &rhs_weight) {
		return lhs.weight < rhs_weight;
	}

private:
};

typedef std::vector<weighted_couple_info_t> weighted_couples_info_t;

struct weights_t {
	weights_t(const kora::config_t &config, size_t groups_count_, bool ns_is_static_);

	weights_t(weights_t &&other);

	couple_info_t
	get(uint64_t size) const;

	weighted_couples_info_t
	get_all(uint64_t size) const;

	const couples_info_t &
	data() const;

	void
	set_coefficient(group_t couple_id, double coefficient);

private:
	typedef std::mutex mutex_t;
	typedef std::lock_guard<mutex_t> lock_guard_t;

	static
	couples_info_t
	create(const kora::config_t &config, size_t groups_count, bool ns_is_static);

	const size_t groups_count;
	couples_info_t couples_info;
	mutable mutex_t couples_info_mutex;
};

} // namespace weight
} // namespace ns_state
} // namespace mastermind

#endif /* LIBMASTERMIND__SRC__COUPLE_WEIGHTS_P__HPP */

