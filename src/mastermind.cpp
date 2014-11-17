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

#include "libmastermind/mastermind.hpp"

#include "mastermind_impl.hpp"

#include <jsoncpp/json.hpp>

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <limits>
#include <sstream>

namespace mastermind {

mastermind_t::mastermind_t(
		const remotes_t &remotes,
		const std::shared_ptr<cocaine::framework::logger_t> &logger,
		int group_info_update_period
		)
	: m_data(new data(remotes, logger, group_info_update_period,
				"/var/tmp/libmastermind.cache",
				std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), "mastermind",
				4000, 4000))
{
}

mastermind_t::mastermind_t(
		const std::string &host,
		uint16_t port,
		const std::shared_ptr<cocaine::framework::logger_t> &logger,
		int group_info_update_period
		)
	: m_data(new data(remotes_t{std::make_pair(host, port)},
				logger, group_info_update_period, "/var/tmp/libmastermind.cache",
				std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), "mastermind",
				4000, 4000))
{
}

mastermind_t::mastermind_t(
		const remotes_t &remotes,
		const std::shared_ptr<cocaine::framework::logger_t> &logger,
		int group_info_update_period,
		std::string cache_path,
		int expired_time,
		std::string worker_name
		)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				std::numeric_limits<int>::max(), expired_time, std::move(worker_name),
				4000, 4000))
{
}

mastermind_t::mastermind_t(const remotes_t &remotes,
		const std::shared_ptr<cocaine::framework::logger_t> &logger,
		int group_info_update_period, std::string cache_path,
		int warning_time, int expire_time,
		std::string worker_name,
		int enqueue_timeout,
		int reconnect_timeout)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				warning_time, expire_time, std::move(worker_name),
				enqueue_timeout, reconnect_timeout))
{
}

mastermind_t::~mastermind_t()
{
	m_data->stop();
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count, const std::string &name_space, uint64_t size) {
	try {
		auto cache = m_data->namespaces_states.copy(name_space);
		auto couple = cache->weights.get(count, size);

		{
			std::ostringstream oss;
			oss
				<< "libmastermind: get_metabalancer_groups: request={group-count=" << count
				<< ", namespace=" << name_space << ", size=" << size
				<< "}; response={"
				<< "couple=[";
			{
				auto &&groups = std::get<0>(couple);
				for (auto beg = groups.begin(), it = beg, end = groups.end(); it != end; ++it) {
					if (beg != it) oss << ", ";
					oss << *it;
				}
			}
			oss << "], weight=" << std::get<1>(couple) << ", free-space=" << std::get<2>(couple)
				<< "};";

			auto msg = oss.str();
			COCAINE_LOG_INFO(m_data->m_logger, "%s", msg.c_str());
		}

		return std::get<0>(couple);
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: cannot obtain couple for: count = %llu; namespace = %s; size = %llu; \"%s\"",
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

			m_data->enqueue("get_group_info", group, resp);
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
		auto cache = m_data->fake_groups_info.copy();

		std::map<int, std::vector<int>> result;

		for (auto it = cache->begin(), end = cache->end(); it != end; ++it) {
			result.insert(std::make_pair(it->first, it->second.groups));
		}

		return result;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(m_data->m_logger, "libmastermind: get_symmetric_groups: \"%s\"", ex.code().message().c_str());
		throw;
	}
}

std::vector<int> mastermind_t::get_symmetric_groups(int group) {
	std::vector<int> result = get_couple_by_group(group);

	if (result.empty()) {
		result = {group};
	}

	if (m_data->m_logger->verbosity() >= cocaine::logging::debug) {
		std::ostringstream oss;
		oss
			<< "libmastermind: get_symmetric_groups: request={group=" << group
			<< "}; response={"
			<< "couple=[";
		{
			auto &&groups = result;
			for (auto beg = groups.begin(), it = beg, end = groups.end(); it != end; ++it) {
				if (beg != it) oss << ", ";
				oss << *it;
			}
		}
		oss << "]};";

		auto msg = oss.str();
		COCAINE_LOG_DEBUG(m_data->m_logger, "%s", msg.c_str());
	}

	return result;
}

std::vector<int> mastermind_t::get_couple_by_group(int group) {
	auto cache = m_data->fake_groups_info.copy();
	std::vector<int> result;

	auto git = cache->find(group);

	if (git != cache->end()) {
		result = git->second.groups;
	}

	return result;
}

std::vector<int> mastermind_t::get_couple(int couple_id, const std::string &ns) {
	COCAINE_LOG_INFO(m_data->m_logger, "libmastermind: get_couple: couple_id=%d ns=%s"
			, couple_id, ns);

	auto cache = m_data->fake_groups_info.copy();
	std::vector<int> result;

	auto git = cache->find(couple_id);

	if (git == cache->end()) {
		COCAINE_LOG_ERROR(m_data->m_logger
				, "libmastermind: get_couple: cannot find couple by the couple_id");
		return std::vector<int>();
	}

	if (git->second.group_status !=
			namespace_state_init_t::data_t::couples_t::group_info_t::status_tag::COUPLED) {
		COCAINE_LOG_ERROR(m_data->m_logger
				, "libmastermind: get_couple: couple status is not COUPLED: %d"
				, static_cast<int>(git->second.group_status));
		return std::vector<int>();
	}

	if (git->second.ns != ns) {
		COCAINE_LOG_ERROR(m_data->m_logger
				, "libmastermind: get_couple: couple belongs to another namespace: %s"
				, git->second.ns);
		return std::vector<int>();
	}

	{
		std::ostringstream oss;
		oss << "libmastermind: get_couple: couple was found: [";
		{
			const auto &couple = git->second.groups;
			for (auto beg = couple.begin(), it = beg, end = couple.end(); it != end; ++it) {
				if (beg != it) oss << ", ";
				oss << *it;
			}
		}
		oss << "]};";

		auto msg = oss.str();
		COCAINE_LOG_INFO(m_data->m_logger, "%s", msg.c_str());
	}

	return git->second.groups;
}

std::vector<std::vector<int> > mastermind_t::get_bad_groups() {
	try {
		auto cache = m_data->bad_groups.copy();
		return *cache;
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
	try {
		auto cache = m_data->cache_groups.copy();

		auto it = cache->find(key);
		if (it != cache->end())
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
	try {
		auto cache = m_data->namespaces_settings.copy();
		return *cache;
	} catch(const std::system_error &ex) {
		COCAINE_LOG_ERROR(
			m_data->m_logger,
			"libmastermind: get_namespaces_settings; \"%s\"",
			ex.code().message().c_str());
		throw;
	}
}

std::vector<std::string> mastermind_t::get_elliptics_remotes() {
	auto cache = m_data->elliptics_remotes.copy();
	return *cache;
}

std::vector<std::tuple<std::vector<int>, uint64_t, uint64_t>> mastermind_t::get_couple_list(
		const std::string &ns) {
	auto namespace_states = m_data->namespaces_states.copy(ns);
	const auto &weights = namespace_states->weights.data();

	std::map<int, std::tuple<std::vector<int>, uint64_t, uint64_t>> result_map;

	for (auto it = weights.begin(), end = weights.end(); it != end; ++it) {
		auto weight = std::get<1>(*it);
		auto memory = std::get<2>(*it);
		const auto &couple = std::get<0>(*it);
		auto group_id = *std::min_element(couple.begin(), couple.end());

		result_map.insert(std::make_pair(group_id
					, std::make_tuple(couple, weight, memory)));
	}

	{
		const auto &couples = namespace_states->couples.couple_info_map;

		for (auto it = couples.begin(), end = couples.end(); it != end; ++it) {
			const auto &couple_info = it->second;
			const auto &groups = couple_info.groups;

			auto couple_id = *std::min_element(groups.begin(), groups.end());

			result_map.insert(std::make_pair(couple_id
						, std::make_tuple(groups, 0, couple_info.free_effective_space)));
		}
	}

	std::vector<std::tuple<std::vector<int>, uint64_t, uint64_t>> result;
	result.reserve(result_map.size());

	for (auto it = result_map.begin(), end = result_map.end(); it != end; ++it) {
		result.emplace_back(std::move(it->second));
	}

	return result;
}

uint64_t mastermind_t::free_effective_space_in_couple_by_group(size_t group) {
	auto cache = m_data->fake_groups_info.copy();

	auto git = cache->find(group);
	if (git == cache->end()) {
		return 0;
	}

	return git->second.free_effective_space;
}

std::string mastermind_t::json_group_weights() {
	// TODO:
	throw std::runtime_error("Not Implemented");
}

std::string mastermind_t::json_symmetric_groups() {
	// TODO:
	throw std::runtime_error("Not Implemented");
}

std::string mastermind_t::json_bad_groups() {
	auto cache = m_data->bad_groups.copy();

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
	auto cache = m_data->cache_groups.copy();

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

std::string mastermind_t::json_metabalancer_info() {
	// TODO:
	throw std::runtime_error("Not Implemented");
}

std::string mastermind_t::json_namespaces_settings() {
	auto cache = m_data->namespaces_settings.copy();

	Json::Value json;

	for (auto bit = cache->begin(), it = bit; it != cache->end(); ++it) {

		Json::Value json_ns(Json::objectValue);

		json_ns["groups-count"] = it->groups_count();
		json_ns["success-copies-num"] =it->success_copies_num();

		json_ns["auth-keys"]["write"] = it->auth_key_for_write();
		json_ns["auth-keys"]["read"] = it->auth_key_for_read();


		{
			Json::Value json_sc(Json::arrayValue);
			for (auto sc_it = it->static_couple().begin()
					, sc_end = it->static_couple().end(); sc_it != sc_end; ++sc_it) {
				json_sc.append(*sc_it);
			}

			json_ns["static-couple"] = json_sc;
		}

		json_ns["signature"]["token"] = it->sign_token();
		json_ns["signature"]["path-prefix"] = it->sign_path_prefix();
		json_ns["signature"]["port"] = it->sign_port();

		json_ns["is-active"] = it->is_active();

		json_ns["features"]["can-choose-couple-for-upload"] = it->can_choose_couple_to_upload();
		json_ns["features"]["multipart"]["content-length-threshold"] =
			static_cast<Json::Value::Int64>(it->multipart_content_length_threshold());

		json_ns["redirect"]["expire-time"] = it->redirect_expire_time();
		json_ns["redirect"]["content-length-threshold"] =
			static_cast<Json::Value::Int64>(it->redirect_content_length_threshold());

		json[it->name()] = json_ns;
	}

	Json::StyledWriter writer;
	return writer.write(json);
}

std::string mastermind_t::json_namespace_statistics(const std::string &ns) {
	auto cache = m_data->namespaces_statistics.copy();

	auto nit = cache->find(ns);

	if (nit == cache->end()) {
		return std::string();
	}

	std::ostringstream oss;
	oss << "{" << std::endl;

	for (auto begin = nit->second.begin(), it = begin, end = nit->second.end(); it != end; ++it) {
		if (it != begin) {
			oss << "," << std::endl;
		}

		oss << "\t\"" << it->first << "\": " << it->second;
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

void
mastermind_t::set_update_cache_ext1_callback(const std::function<void (bool)> &callback) {
	m_data->set_update_cache_ext1_callback(callback);
}

} // namespace mastermind
