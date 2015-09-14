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
#include "couple_weights_p.hpp"

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
		settings_t(const std::string &name, const kora::config_t &config
				, const user_settings_factory_t &factory);

		settings_t(settings_t &&other);

		size_t groups_count;
		std::string success_copies_num;

		struct auth_keys_t {
			std::string read;
			std::string write;
		} auth_keys;

		groups_t static_groups;

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
			enum class status_tag {
				UNKNOWN, COUPLED
			};

			group_t id;

			status_tag status;

			couple_info_map_iterator_t couple_info_map_iterator;
		};

		struct couple_info_t {
			enum class status_tag {
				UNKNOWN, BAD
			};

			std::string id;

			groups_t groups;
			status_tag status;
			uint64_t free_effective_space;

			kora::dynamic_t hosts;

			std::vector<group_info_map_iterator_t> groups_info_map_iterator;
		};

		couples_t(const kora::config_t &config);

		couples_t(couples_t &&other);

		group_info_map_t group_info_map;
		couple_info_map_t couple_info_map;
	};

	struct statistics_t {
		statistics_t(const kora::config_t &config);

		bool
		ns_is_full() const;

	private:
		bool is_full;
	};

	/*struct weights_t {
		weights_t(const kora::config_t &config, size_t groups_count_) {}
		groups_t get(size_t size) const { return groups_t(); }
	};*/

	data_t(std::string name, const kora::config_t &config
			, const user_settings_factory_t &factory);

	data_t(data_t &&other);

	void check_consistency();

	std::string name;

	settings_t settings;
	couples_t couples;
	ns_state::weight::weights_t weights;
	statistics_t statistics;

	std::string extract;
};

class namespace_state_init_t
	: public namespace_state_t {
public:
	namespace_state_init_t(std::shared_ptr<data_t> data_);

	struct data_t : namespace_state_t::data_t {
		data_t(std::string name, const kora::config_t &config
				, const user_settings_factory_t &factory);

		data_t(data_t &&other);
	};

};


} // namespace mastermind

#endif /* SRC__NAMESPACE_STATE_P__HPP */

