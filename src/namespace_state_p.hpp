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

#ifndef SRC__NAMESPACE_STATE_P__HPP
#define SRC__NAMESPACE_STATE_P__HPP

#include "libmastermind/mastermind.hpp"

#include "cocaine/traits/dynamic.hpp"

#include <kora/config.hpp>

#include <tuple>
#include <map>
#include <vector>
#include <functional>

namespace mastermind {

class namespace_state_t::data_t {
public:
	struct settings_t {
		settings_t(const kora::config_t &config, const user_settings_factory_t &factory);

		size_t groups_count;
		std::string success_copies_num;

		struct {
			std::string read;
			std::string write;
		} auth_keys;

		user_settings_ptr_t user_settings_ptr;
	};

	struct couples_t {
		struct group_info_t;
		struct couple_info_t;

		typedef std::map<int, group_info_t> group_info_map_t;
		typedef std::map<std::string, couple_info_t> couple_info_map_t;

		typedef group_info_map_t::const_iterator group_info_map_iterator_t;
		typedef couple_info_map_t::const_iterator couple_info_map_iterator_t;

		struct group_info_t {
			group_t id;

			couple_info_map_iterator_t couple_info_map_iterator;
		};

		struct couple_info_t {
			std::string id;

			groups_t groups;

			std::vector<group_info_map_iterator_t> groups_info_map_iterator;
		};

		couples_t(const kora::config_t &config);

		group_info_map_t group_info_map;
		couple_info_map_t couple_info_map;
	};

	struct weights_t {
		typedef std::tuple<groups_t, uint64_t, uint64_t> couple_with_info_t;
		typedef std::vector<couple_with_info_t> couples_with_info_t;

		weights_t(const kora::config_t &config, size_t groups_count_);

		void
		set(couples_with_info_t couples_with_info_);

		couple_with_info_t
		get(size_t groups_count_, uint64_t size) const;

	private:
		typedef std::reference_wrapper<const couple_with_info_t> const_couple_ref_t;
		typedef std::map<uint64_t, const_couple_ref_t> weighted_couples_t;
		typedef std::map<uint64_t, weighted_couples_t> couples_by_avalible_memory_t;

		static
		bool
		couples_with_info_cmp(const couple_with_info_t &lhs, const couple_with_info_t &rhs);

		size_t groups_count;
		couples_with_info_t couples_with_info;
		couples_by_avalible_memory_t couples_by_avalible_memory;
	};

	struct statistics_t {
		statistics_t(const kora::config_t &config);
	};

	data_t(std::string name, const kora::config_t &config
			, const user_settings_factory_t &factory);

	std::string name;

	settings_t settings;
	couples_t couples;
	weights_t weights;
	statistics_t statistics;
};

class namespace_state_init_t
	: public namespace_state_t {
public:
	namespace_state_init_t(std::shared_ptr<const data_t> data_);

	struct data_t : namespace_state_t::data_t {
		data_t(std::string name, const kora::config_t &config
				, const user_settings_factory_t &factory);
	};

};


} // namespace mastermind

#endif /* SRC__NAMESPACE_STATE_P__HPP */

