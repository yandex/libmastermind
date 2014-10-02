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
		auto cache = m_data->namespaces_weights.copy(name_space, count);
		auto couple = cache->get_couple(size);

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
		auto cache = m_data->symmetric_groups.copy();
		return *cache;
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
	auto cache = m_data->metabalancer_info.copy();
	std::vector<int> result;

	auto git = cache->group_info_map.find(group);

	if (git != cache->group_info_map.end()) {
		auto r = git->second->couple;
		// TODO: use uniform types
		result.assign(r.begin(), r.end());
	}

	return result;
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
	auto cache_weights = m_data->namespaces_weights.copy(ns);
	auto cache_couple_list = m_data->metabalancer_info.copy();

	std::map<int, std::tuple<std::vector<int>, uint64_t, uint64_t>> result_map;

	for (auto cw_it = cache_weights.begin(), cw_end = cache_weights.end();
			cw_it != cw_end; ++cw_it) {
		const auto &weights = cw_it->second.get_value()->data();

		for (auto it = weights.begin(), end = weights.end(); it != end; ++it) {
			auto weight = std::get<1>(*it);
			auto memory = std::get<2>(*it);
			const auto &couple = std::get<0>(*it);
			auto group_id = *std::min_element(couple.begin(), couple.end());

			result_map.insert(std::make_pair(group_id
						, std::make_tuple(couple, weight, memory)));
		}
	}

	{
		auto couple_list = cache_couple_list->namespace_info_map[ns];

		for (auto it = couple_list.begin(), end = couple_list.end(); it != end; ++it) {
			auto couple_info = it->lock();

			const auto &couple = couple_info->tuple;
			auto group_id = *std::min_element(couple.begin(), couple.end());

			result_map.insert(std::make_pair(group_id
						, std::make_tuple(couple, static_cast<uint64_t>(0)
							, couple_info->free_effective_space)));
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
	auto cache = m_data->metabalancer_info.copy();

	auto git = cache->group_info_map.find(group);
	if (git == cache->group_info_map.end()) {
		return 0;
	}

	if (auto p = git->second->couple_info.lock()) {
		return p->free_effective_space;
	}

	return 0;
}

std::string mastermind_t::json_group_weights() {
	auto cache = m_data->namespaces_weights.copy();

	Json::Value json;

	for (auto c_it = cache.begin(), c_end = cache.end(); c_it != c_end; ++c_it) {
		const auto &name = std::get<0>(c_it->first);
		const auto &groups_count = std::get<1>(c_it->first);

		const auto &couples = c_it->second.get_value()->data();

		Json::Value json_ns_gc(Json::objectValue);

		for (auto it = couples.begin(), end = couples.end(); it != end; ++it) {
			const auto &couple = std::get<0>(*it);

			std::ostringstream oss;
			oss << '[';
			for (auto begin_it = couple.begin(), it = begin_it, end_it = couple.end();
					it != end_it; ++it) {
				if (begin_it != it) oss << ", ";
				oss << *it;
			}
			oss << ']';

			const auto &weight = std::get<1>(*it);
			const auto &available_memory = std::get<2>(*it);

			auto ns_gc_name = oss.str();
			json_ns_gc[ns_gc_name]["weight"] = static_cast<Json::Value::UInt64>(weight);
			json_ns_gc[ns_gc_name]["space"] = static_cast<Json::Value::UInt64>(available_memory);
		}

		json[name][boost::lexical_cast<std::string>(groups_count)] = json_ns_gc;
	}

	Json::StyledWriter writer;
	return writer.write(json);
}

std::string mastermind_t::json_symmetric_groups() {
	auto cache = m_data->symmetric_groups.copy();

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
	auto cache = m_data->metabalancer_info.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;
	oss << "\t[" << std::endl;

	for (auto cit_beg = cache->couple_info_map.begin(),
			cit_end = cache->couple_info_map.end(),
			cit = cit_beg;
			cit != cit_end; ++cit)
	{
		if (cit != cit_beg) {
			oss << ',' << std::endl;
		}
		oss << "\t\t{" << std::endl;
		oss << "\t\t\t\"id\" : \"" << cit->second->id << "\"," << std::endl;
		oss << "\t\t\t\"free_effective_space\" : " << cit->second->free_effective_space << ',' << std::endl;
		oss << "\t\t\t\"free_space\" : " << cit->second->free_space << ',' << std::endl;
		oss << "\t\t\t\"used_space\" : " << cit->second->used_space << ',' << std::endl;
		oss << "\t\t\t\"namespace\" : \"" << cit->second->ns << "\"" << std::endl;
		oss << "\t\t\t\"couple_status\" : ";
		switch (cit->second->couple_status) {
		case couple_info_t::OK:
			oss << "\"OK\"";
			break;
		default:
			oss << "\"UNKNOWN\"";
		}
		oss << std::endl << "\t\t}";
	}

	oss << std::endl << "\t]" << std::endl << "}";

	return oss.str();
}

std::string mastermind_t::json_namespaces_settings() {
	auto cache = m_data->namespaces_settings.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;

	for (auto bit = cache->begin(), it = bit; it != cache->end(); ++it) {
		if (it != bit) oss << "," << std::endl;

		oss << "\t\"" << it->name() << "\" : {" << std::endl;

		oss << "\t\t\"groups-count\" : " << it->groups_count() << "," << std::endl;
		oss << "\t\t\"success-copies-num\" : \"" << it->success_copies_num() << "\"," << std::endl;

		oss << "\t\t\"auth-keys\" : {" << std::endl;
		oss << "\t\t\t\"write\" : \"" << it->auth_key_for_write() << "\"," << std::endl;
		oss << "\t\t\t\"read\" : \"" << it->auth_key_for_read() << "\"" << std::endl;
		oss << "\t\t}," << std::endl;

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
