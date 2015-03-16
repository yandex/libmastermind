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

#include "namespace_state_p.hpp"

size_t
mastermind::namespace_state_t::settings_t::groups_count() const {
	return namespace_state.data->settings.groups_count;
}

const std::string &
mastermind::namespace_state_t::settings_t::success_copies_num() const {
	return namespace_state.data->settings.success_copies_num;
}

const mastermind::namespace_state_t::user_settings_t &
mastermind::namespace_state_t::settings_t::user_settings() const {
	if (!namespace_state.data->settings.user_settings_ptr) {
		throw std::runtime_error("uninitialized user settings cannot be used");
	}

	return *namespace_state.data->settings.user_settings_ptr;
}

mastermind::namespace_state_t::settings_t::settings_t(const namespace_state_t &namespace_state_)
	: namespace_state(namespace_state_)
{
}

mastermind::groups_t
mastermind::namespace_state_t::couples_t::get_couple_groups(group_t group) const {
	auto it = namespace_state.data->couples.group_info_map.find(group);

	if (it == namespace_state.data->couples.group_info_map.end()) {
		return groups_t();
	}

	return it->second.couple_info_map_iterator->second.groups;
}

mastermind::groups_t
mastermind::namespace_state_t::couples_t::get_groups(group_t group) const {
	auto groups = get_couple_groups(group);

	if (groups.empty()) {
		return {group};
	}
	return groups;
}

uint64_t
mastermind::namespace_state_t::couples_t::free_effective_space(group_t group) const {
	auto it = namespace_state.data->couples.group_info_map.find(group);

	if (it == namespace_state.data->couples.group_info_map.end()) {
		return 0;
	}

	return it->second.couple_info_map_iterator->second.free_effective_space;
}

mastermind::namespace_state_t::couples_t::couples_t(const namespace_state_t &namespace_state_)
	: namespace_state(namespace_state_)
{
}

mastermind::groups_t
mastermind::namespace_state_t::weights_t::groups(uint64_t size) const {
	return namespace_state.data->weights.get(size).groups;
}

mastermind::namespace_state_t::weights_t::weights_t(const namespace_state_t &namespace_state_)
	: namespace_state(namespace_state_)
{
}

mastermind::namespace_state_t::settings_t
mastermind::namespace_state_t::settings() const {
	return settings_t(*this);
}

mastermind::namespace_state_t::couples_t
mastermind::namespace_state_t::couples() const {
	return couples_t(*this);
}

mastermind::namespace_state_t::weights_t
mastermind::namespace_state_t::weights() const {
	return weights_t(*this);
}

const std::string &mastermind::namespace_state_t::name() const {
	return data->name;
}

mastermind::namespace_state_t::operator bool() const {
	return static_cast<bool>(data);
}

