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

#ifndef LIBMASTERMIND__SRC__CACHED_KEY__HPP
#define LIBMASTERMIND__SRC__CACHED_KEY__HPP

#include "libmastermind/common.hpp"

#include <kora/dynamic.hpp>

#include <boost/lexical_cast.hpp>

#include <string>
#include <map>

namespace mastermind {

class cached_keys_t {
public:
	cached_keys_t()
	{
	}

	cached_keys_t(const kora::dynamic_t &dynamic)
		: groups_map(create_groups_map(dynamic))
	{
	}

	groups_t
	get(const std::string &key, const std::string &couple_id) const {
		auto key_it = groups_map.find(key);

		if (groups_map.end() == key_it) {
			return {};
		}

		auto couple_id_it = key_it->second.find(couple_id);

		if (key_it->second.end() == couple_id_it) {
			return {};
		}

		return couple_id_it->second;
	}

	groups_t
	get(const std::string &key, group_t couple_id) const {
		return get(key, boost::lexical_cast<std::string>(couple_id));
	}

private:
	typedef std::map<std::string, std::map<std::string, groups_t>> groups_map_t;

	static
	groups_map_t
	create_groups_map(const kora::dynamic_t &dynamic) {
		try {
			const auto &dynamic_keys = dynamic.as_object();

			auto groups_map = groups_map_t{};
			for (auto key_it = dynamic_keys.begin(), key_end = dynamic_keys.end()
					; key_it != key_end; ++key_it) {
				const auto &dynamic_couple_ids = key_it->second.as_object();

				auto couple_id_map = std::map<std::string, groups_t>{};
				for (auto couple_id_it = dynamic_couple_ids.begin()
						, couple_id_end = dynamic_couple_ids.end()
						; couple_id_it != couple_id_end; ++couple_id_it) {
					const auto &dynamic_info = couple_id_it->second.as_object();
					const auto &dynamic_cache_groups = dynamic_info.at("cache_groups").as_array();

					auto groups = groups_t{};
					for (auto cache_groups_it = dynamic_cache_groups.begin()
							, cache_groups_end = dynamic_cache_groups.end()
							; cache_groups_it != cache_groups_end; ++cache_groups_it) {
						groups.emplace_back(cache_groups_it->to<group_t>());
					}
					couple_id_map.insert(std::make_pair(couple_id_it->first, groups));
				}

				groups_map.insert(std::make_pair(key_it->first, couple_id_map));
			}

			return groups_map;
		} catch (const std::exception &ex) {
			throw std::runtime_error(std::string("cached_keys parse error: ") + ex.what());
		}
	}

	groups_map_t groups_map;
};

} // namespace mastermind

#endif /* LIBMASTERMIND__SRC__CACHED_KEY__HPP */

