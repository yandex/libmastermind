// libelliptics-proxy - smart proxy for Elliptics file storage
// Copyright (C) 2012 Anton Kortunov <toshik@yandex-team.ru>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "libmastermind/mastermind.hpp"

#include "mastermind_impl.hpp"

namespace mastermind {

mastermind_t::mastermind_t(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period)
	: m_data(new data(remotes, logger, group_info_update_period))
{
}

mastermind_t::mastermind_t(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period)
	: m_data(new data(host, port, logger, group_info_update_period))
{
}

mastermind_t::~mastermind_t()
{
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count, const std::string &name_space, uint64_t size) {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_metabalancer_groups_info.mutex);
		(void) lock;

		if (m_data->m_metabalancer_groups_info.cache->empty()) {
			m_data->collect_group_weights();
		}

		return m_data->m_metabalancer_groups_info.cache->get_couple(count, name_space, size);
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_metabalancer_groups: count = %llu; namespace = %s; size = %llu; \"%s\"",
			count, name_space.c_str(), size, ex.code().message().c_str());
		throw;
	}
}

group_info_response_t mastermind_t::get_metabalancer_group_info(int group) {
	try {
		group_info_response_t resp;

		{
			std::lock_guard<std::mutex> lock(m_data->m_mutex);
			(void) lock;

			m_data->retry(&cocaine::framework::app_service_t::enqueue<decltype(group)>, resp, "get_group_info", group);
		}
		return resp;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_metabalancer_group_info: group = %d; \"%s\"",
			group, ex.code().message().c_str());
		throw;
	}
}

std::map<int, std::vector<int>> mastermind_t::get_symmetric_groups() {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_symmetric_groups.mutex);
		(void) lock;

		if (m_data->m_symmetric_groups.cache->empty()) {
			m_data->collect_symmetric_groups();
		}

		return *m_data->m_symmetric_groups.cache;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_symmetric_groups: \"%s\"", ex.code().message().c_str());
		throw;
	}
}

std::vector<int> mastermind_t::get_symmetric_groups(int group) {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_symmetric_groups.mutex);
		(void) lock;

		if (m_data->m_symmetric_groups.cache->empty()) {
			m_data->collect_symmetric_groups();
		}

		auto it = m_data->m_symmetric_groups.cache->find(group);
		if (it == m_data->m_symmetric_groups.cache->end()) {
			return std::vector<int>();
		}
		return it->second;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_symmetric_groups: group = %d; \"%s\"",
			group, ex.code().message().c_str());
		throw;
	}
}

std::vector<std::vector<int> > mastermind_t::get_bad_groups() {
	try {
		std::lock_guard<std::recursive_mutex> lock(m_data->m_bad_groups.mutex);
		(void) lock;

		if (m_data->m_bad_groups.cache->empty()) {
			m_data->collect_bad_groups();
		}

		return *m_data->m_bad_groups.cache;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_bad_groups: \"%s\"", ex.code().message().c_str());
		throw;
	}
}

std::vector<int> mastermind_t::get_all_groups() {
	std::vector<int> res;

	auto groups = get_symmetric_groups();
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		res.push_back(it->first);
	}

	return res;
}

std::vector<int> mastermind_t::get_cache_groups(const std::string &key) {
	std::lock_guard<std::recursive_mutex> lock(m_data->m_cache_groups.mutex);
	(void) lock;

	try {
		if (m_data->m_cache_groups.cache->empty()) {
			m_data->collect_cache_groups();
		}

		auto it = m_data->m_cache_groups.cache->find(key);
		if (it != m_data->m_cache_groups.cache->end())
			return it->second;
		return std::vector<int>();
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_cache_groups: key = %s; \"%s\"",
			key.c_str(), ex.code().message().c_str());
		throw;
	}
}

std::vector<namespace_settings_t> mastermind_t::get_namespaces_settings() {
	std::lock_guard<std::recursive_mutex> lock(m_data->m_namespaces_settings.mutex);
	(void) lock;

	try {
		if (m_data->m_namespaces_settings.cache->empty()) {
			m_data->collect_namespaces_settings();
		}

		return *m_data->m_namespaces_settings.cache;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_namespaces_settings; \"%s\"",
			ex.code().message().c_str());
		throw;
	}
}

std::string mastermind_t::json_group_weights() {
	auto cache = m_data->m_metabalancer_groups_info.copy();
	return cache->to_string();
}

std::string mastermind_t::json_symmetric_groups() {
	auto cache = m_data->m_symmetric_groups.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;
	auto ite = cache->end();
	if (cache->begin() != cache->end()) --ite;
	for (auto it = cache->begin(); it != cache->end(); ++it) {
		oss << "\t\"" << it->first << "\" : [";
		for (auto it2b = it->second.begin(), it2 = it2b; it2 != it->second.end(); ++it2) {
			if (it2 != it2b) {
				oss << ", ";
			}
			oss << *it2;
		}
		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

std::string mastermind_t::json_bad_groups() {
	auto cache = m_data->m_bad_groups.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;
	auto ite = cache->end();
	if (cache->begin() != cache->end()) --ite;
	for (auto it = cache->begin(); it != cache->end(); ++it) {
		oss << "\t[";
		for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
			if (it2 != it->begin()) {
				oss << ", ";
			}
			oss << *it2;
		}
		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

std::string mastermind_t::json_cache_groups() {
	auto cache = m_data->m_cache_groups.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;
	auto ite = cache->end();
	if (cache->begin() != cache->end()) --ite;
	for (auto it = cache->begin(); it != cache->end(); ++it) {
		oss << "\t\"" << it->first << "\" : [";
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
			if (it2 != it->second.begin()) {
				oss << ", ";
			}
			oss << *it2;
		}

		oss << "]";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";

	return oss.str();
}

std::string mastermind_t::json_namespaces_settings() {
	auto cache = m_data->m_namespaces_settings.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;

	for (auto bit = cache->begin(), it = bit; it != cache->end(); ++it) {
		if (it != bit) oss << "," << std::endl;

		oss << "\t\"" << it->name() << "\" : {" << std::endl;

		oss << "\t\t\"groups-count\" : " << it->groups_count() << "," << std::endl;
		oss << "\t\t\"success-copies-num\" : \"" << it->success_copies_num() << "\"," << std::endl;
		oss << "\t\t\"auth-key\" : \"" << it->auth_key() << "\"," << std::endl;
		oss << "\t\t\"static-couple\" : [";

		for (auto bcit = it->static_couple().begin(), cit = bcit; cit != it->static_couple().end(); ++cit) {
			if (cit != bcit) oss << ", ";
			oss << *cit;
		}

		oss << "]" << std::endl;

		oss << "\t\t\"signature\" : {" << std::endl;
		oss << "\t\t\t\"token\" : \"" << it->sign_token() << "\"," << std::endl;
		oss << "\t\t\t\"path_prefix\" : \"" << it->sign_path_prefix() << "\"," << std::endl;
		oss << "\t\t\t\"port\" : \"" << it->sign_port() << "\"," << std::endl;
		oss << "\t\t}" << std::endl;

		oss << "\t}";
	}

	oss << std::endl << "}";

	return oss.str();
}

void mastermind_t::cache_force_update() {
	m_data->cache_force_update();
}

void mastermind_t::set_update_cache_callback(const std::function<void (void)> &callback) {
	m_data->set_update_cache_callback(callback);
}

} // namespace mastermind
