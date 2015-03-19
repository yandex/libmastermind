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

#include "couple_weights_p.hpp"
#include "utils.hpp"

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace mastermind {
namespace ns_state {
namespace weight {

bool
memory_comparator(const couple_info_t &lhs, const couple_info_t &rhs) {
	return lhs.memory > rhs.memory;
}

weights_t::weights_t(const kora::config_t &config
		, size_t groups_count_)
	try
	: groups_count(groups_count_)
	, couples_info(create(config, groups_count))
{
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create weights-state: ") + ex.what());
}

weights_t::weights_t(weights_t &&other)
	: groups_count(other.groups_count)
	, couples_info(std::move(other.couples_info))
{
}

couples_info_t
weights_t::create(
		const kora::config_t &config, size_t groups_count) {
	const auto &couples = config.at(boost::lexical_cast<std::string>(groups_count))
		.underlying_object().as_array();

	couples_info_t couples_info;

	for (auto it = couples.begin(), end = couples.end(); it != end; ++it) {
		const auto &couple = it->as_array();
		couple_info_t couple_info;

		{
			const auto &dynamic_groups = couple[0].as_array();

			for (auto it = dynamic_groups.begin(), end = dynamic_groups.end();
					it != end; ++it) {
				couple_info.groups.emplace_back(it->to<group_t>());
			}
		}

		if (couple_info.groups.size() != groups_count) {
			std::ostringstream oss;
			oss
				<< "groups.size is not equeal for groups_count(" << groups_count
				<< "), groups=" << couple_info.groups;
			throw std::runtime_error(oss.str());
		}

		couple_info.weight = couple[1].to<uint64_t>();
		couple_info.memory = couple[2].to<uint64_t>();
		couple_info.id = *std::min_element(couple_info.groups.begin(), couple_info.groups.end());

		couples_info.emplace_back(std::move(couple_info));
	}

	std::sort(couples_info.begin(), couples_info.end(), memory_comparator);

	return couples_info;
}

couple_info_t
weights_t::get(uint64_t size) const {
	auto weighted_groups = get_all(size);

	auto total_weight = weighted_groups.back().weight;
	double shoot_point = double(random()) / RAND_MAX * total_weight;
	auto it = std::lower_bound(weighted_groups.begin(), weighted_groups.end()
			, uint64_t(shoot_point));

	if (it == weighted_groups.end()) {
		throw couple_not_found_error();
	}

	return *it->couple_info;
}

weighted_couples_info_t
weights_t::get_all(uint64_t size) const {
	weighted_couples_info_t weighted_couples_info;
	weighted_couples_info.reserve(couples_info.size());
	uint64_t total_weight = 0;

	{
		lock_guard_t lock_guard(couples_info_mutex);
		for (auto it = couples_info.begin(), end = couples_info.end();
				it != end; ++it) {

			if (it->memory < size) {
				break;
			}

			uint64_t weight = it->weight * it->coefficient;

			if (weight == 0) {
				continue;
			}

			total_weight += weight;

			weighted_couples_info.emplace_back(total_weight, it);
		}
	}

	if (weighted_couples_info.empty()) {
		throw not_enough_memory_error();
	}

	return weighted_couples_info;
}

const couples_info_t &
weights_t::data() const {
	return couples_info;
}

void
weights_t::set_coefficient(group_t couple_id, double coefficient) {
	for (auto it = couples_info.begin(), end = couples_info.end(); it != end; ++it) {
		const auto &groups = it->groups;
		auto cit = std::find(groups.begin(), groups.end(), couple_id);

		if (cit != groups.end()) {
			lock_guard_t lock_guard(couples_info_mutex);
			it->coefficient = std::min(it->coefficient, coefficient);
			break;
		}
	}
}

} // namespace weight
} // namespace ns_state
} // namespace mastermind

