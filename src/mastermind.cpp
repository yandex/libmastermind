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

#include <algorithm>
#include <limits>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "libmastermind/mastermind.hpp"
#include "mastermind_impl.hpp"

namespace mastermind {

mastermind_t::mastermind_t(
		const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period
		)
	: m_data(new data(remotes, logger, group_info_update_period,
				"/var/tmp/libmastermind.cache",
				std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), "mastermind",
				4000, 4000, namespace_state_t::user_settings_factory_t(), true))
{
}

mastermind_t::mastermind_t(
		const std::string &host,
		uint16_t port,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period
		)
	: m_data(new data(remotes_t{std::make_pair(host, port)},
				logger, group_info_update_period, "/var/tmp/libmastermind.cache",
				std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), "mastermind",
				4000, 4000, namespace_state_t::user_settings_factory_t(), true))
{
}

mastermind_t::mastermind_t(
		const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period,
		std::string cache_path,
		int expired_time,
		std::string worker_name
		)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				std::numeric_limits<int>::max(), expired_time, std::move(worker_name),
				4000, 4000, namespace_state_t::user_settings_factory_t(), true))
{
}

mastermind_t::mastermind_t(const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period, std::string cache_path,
		int warning_time, int expire_time,
		std::string worker_name,
		int enqueue_timeout,
		int reconnect_timeout)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				warning_time, expire_time, std::move(worker_name),
				enqueue_timeout, reconnect_timeout,
				namespace_state_t::user_settings_factory_t(), true))
{
}

mastermind_t::mastermind_t(const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period, std::string cache_path,
		int warning_time, int expire_time,
		std::string worker_name,
		int enqueue_timeout,
		int reconnect_timeout,
		namespace_state_t::user_settings_factory_t user_settings_factory
		)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				warning_time, expire_time, std::move(worker_name),
				enqueue_timeout, reconnect_timeout,
				std::move(user_settings_factory), true))
{
}

mastermind_t::mastermind_t(const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period, std::string cache_path,
		int warning_time, int expire_time,
		std::string worker_name,
		int enqueue_timeout,
		int reconnect_timeout,
		bool auto_start
		)
	: m_data(new data(remotes, logger, group_info_update_period, std::move(cache_path),
				warning_time, expire_time, std::move(worker_name),
				enqueue_timeout, reconnect_timeout,
				namespace_state_t::user_settings_factory_t(), auto_start))
{
}

mastermind_t::~mastermind_t()
{
	if (m_data->is_running()) {
		m_data->stop();
	}
}

void
mastermind_t::start() {
	m_data->start();
}

void
mastermind_t::stop() {
	m_data->stop();
}

bool
mastermind_t::is_running() const {
	return m_data->is_running();
}

bool
mastermind_t::is_valid() const {
	return m_data->is_valid();
}

std::vector<int> mastermind_t::get_metabalancer_groups(uint64_t count, const std::string &name_space, uint64_t size) {
	try {
		auto cache = m_data->namespaces_states.copy(name_space);

		if (count != cache.get_value().settings.groups_count) {
			throw invalid_groups_count_error();
		}

		auto couple = cache.get_value().weights.get(size);

		MM_LOG_INFO(m_data->m_logger, "libmastermind: {}: request={{group-count={}, namespace={}, size={}}}; response={{couple={}, weight={}, free-space={}}};",
			__func__,
			count,
			name_space,
			size,
			couple.groups,
			couple.weight,
			couple.memory
		);

		return couple.groups;

	} catch(const std::system_error &e) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: request={{group-count={}, namespace={}, size={}}}: cannot obtain couple: \"{}\"",
			__func__, count, name_space, size, e.code().message()
		);
		throw;
	}
}

// group_info_response_t mastermind_t::get_metabalancer_group_info(int group) {
// 	try {
// 		group_info_response_t resp;

// 		{
// 			std::lock_guard<std::mutex> lock(m_data->m_mutex);
// 			(void) lock;

// 			m_data->enqueue_old("get_group_info", group, resp);
// 		}
// 		return resp;

// 	} catch(const std::system_error &e) {
// 		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: group={}: \"{}\"", __func__, group, e.code().message());
// 		throw;
// 	}
// }

std::map<int, std::vector<int>> mastermind_t::get_symmetric_groups() {
	try {
		auto cache = m_data->fake_groups_info.copy();

		std::map<int, std::vector<int>> result;

		for (auto it = cache.get_value().begin(), end = cache.get_value().end();
				it != end; ++it) {
			result.insert(std::make_pair(it->first, it->second.groups));
		}

		return result;

	} catch(const std::system_error &e) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: \"{}\"", __func__, e.code().message());
		throw;
	}
}

std::vector<int> mastermind_t::get_symmetric_groups(int group) {
	std::vector<int> result = get_couple_by_group(group);

	if (result.empty()) {
		result = {group};
	}

	MM_LOG_DEBUG(m_data->m_logger, "libmastermind: {}: request={{{}}}; response={{couple={}}};", __func__, group, result);

	return result;
}

std::vector<int> mastermind_t::get_couple_by_group(int group) {
	auto cache = m_data->fake_groups_info.copy();
	std::vector<int> result;

	auto found = cache.get_value().find(group);
	if (found != cache.get_value().end()) {
		result = found->second.groups;
	}

	MM_LOG_DEBUG(m_data->m_logger, "libmastermind: {}: request={{{}}}; response={{couple={}}};", __func__, group, result);

	return result;
}

std::vector<int> mastermind_t::get_couple(int couple_id, const std::string &ns) {
	MM_LOG_INFO(m_data->m_logger, "libmastermind: {}: request={{couple_id={}, ns={}}}", __func__, couple_id, ns);

	auto cache = m_data->fake_groups_info.copy();
	std::vector<int> result;

	auto found = cache.get_value().find(couple_id);

	if (found == cache.get_value().end()) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: request={{couple_id={}, ns={}}}: couple not found", __func__, couple_id, ns);
		return std::vector<int>();
	}

	const auto &couple = found->second;

	if (couple.group_status !=
			namespace_state_init_t::data_t::couples_t::group_info_t::status_tag::COUPLED) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: request={{couple_id={}, ns={}}}: couple status is not COUPLED: status={}", __func__, couple_id, ns, static_cast<int>(couple.group_status));
		return {};
	}

	if (couple.ns != ns) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: request={{couple_id={}, ns={}}}: couple belongs to another namespace: {}", __func__, couple_id, ns, couple.ns);
		return std::vector<int>();
	}

	MM_LOG_INFO(m_data->m_logger, "libmastermind: {}: request={{couple_id={}, ns={}}}: couple found: {}", __func__, couple_id, ns, couple.groups);

	return couple.groups;
}

std::vector<std::vector<int> > mastermind_t::get_bad_groups() {
	try {
		auto cache = m_data->bad_groups.copy();
		return cache.get_value();

	} catch(const std::system_error &e) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: \"{}\"", __func__, e.code().message());
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
	MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: using of obsolete method", __func__);
	return {};
}

std::vector<namespace_settings_t> mastermind_t::get_namespaces_settings() {
	try {
		auto cache = m_data->namespaces_settings.copy();
		return cache.get_value();

	} catch(const std::system_error &e) {
		MM_LOG_ERROR(m_data->m_logger, "libmastermind: {}: \"{}\"",	__func__, e.code().message());
		throw;
	}
}

std::vector<std::string> mastermind_t::get_elliptics_remotes() {
	auto cache = m_data->elliptics_remotes.copy();

	if (cache.is_expired()) {
		return {};
	}

	return cache.get_value();
}

std::vector<std::tuple<std::vector<int>, uint64_t, uint64_t>> mastermind_t::get_couple_list(
		const std::string &ns) {
	auto namespace_states = m_data->namespaces_states.copy(ns);

	if (namespace_states.is_expired()) {
		return std::vector<std::tuple<std::vector<int>, uint64_t, uint64_t>>();
	}

	const auto &weights = namespace_states.get_value().weights.data();

	std::map<int, std::tuple<std::vector<int>, uint64_t, uint64_t>> result_map;

	for (auto it = weights.begin(), end = weights.end(); it != end; ++it) {
		auto weight = it->weight;
		auto memory = it->memory;
		const auto &couple = it->groups;
		auto group_id = it->id;

		result_map.insert(std::make_pair(group_id
					, std::make_tuple(couple, weight, memory)));
	}

	{
		const auto &couples = namespace_states.get_value().couples.couple_info_map;

		for (auto it = couples.begin(), end = couples.end(); it != end; ++it) {
			const auto &couple_info = it->second;
			const auto &groups = couple_info.groups;

			auto couple_id = *std::min_element(groups.begin(), groups.end());

			result_map.insert(std::make_pair(couple_id
						, std::make_tuple(groups, static_cast<uint64_t>(0)
							, couple_info.free_effective_space)));
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

	if (cache.is_expired()) {
		return 0;
	}

	auto git = cache.get_value().find(group);
	if (git == cache.get_value().end()) {
		return 0;
	}

	return git->second.free_effective_space;
}

namespace_state_t
mastermind_t::get_namespace_state(const std::string &name) const {
	auto data = m_data->get_namespace_state(name);
	return namespace_state_init_t(data);
}

namespace_state_t
mastermind_t::find_namespace_state(group_t group) const {
	auto cache = m_data->fake_groups_info.copy();
	auto it = cache.get_value().find(group);

	if (it == cache.get_value().end()) {
		throw namespace_state_not_found_error{};
	}

	return get_namespace_state(it->second.ns);
}

groups_t
mastermind_t::get_cached_groups(const std::string &elliptics_id, group_t couple_id) const {
	auto cache = m_data->cached_keys.copy();
	return cache.get_value().get(elliptics_id, couple_id);
}

std::string mastermind_t::json_group_weights() {
	auto cache = m_data->namespaces_states.copy();

	kora::dynamic_t raw_group_weights = kora::dynamic_t::empty_object;
	auto &raw_group_weights_object = raw_group_weights.as_object();

	for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
		if (it->second.is_expired()) {
			continue;
		}

		const auto &ns_state = it->second.get_value();
		const auto &ns_raw_state = it->second.get_raw_value();

		raw_group_weights_object[ns_state.name] = ns_raw_state.as_object()["weights"];
	}

	return kora::to_pretty_json(raw_group_weights);
}

std::string mastermind_t::json_symmetric_groups() {
	auto cache = m_data->fake_groups_info.copy();

	kora::dynamic_t raw_symmetric_groups = kora::dynamic_t::empty_object;
	auto &raw_symmetric_groups_object = raw_symmetric_groups.as_object();

	for (auto it = cache.get_value().begin(), end = cache.get_value().end(); it != end; ++it) {
		raw_symmetric_groups_object[boost::lexical_cast<std::string>(it->first)]
			= it->second.groups;
	}

	return kora::to_pretty_json(raw_symmetric_groups);
}

std::string mastermind_t::json_bad_groups() {
	auto cache = m_data->bad_groups.copy();

	std::ostringstream oss;
	oss << "{" << std::endl;
	auto ite = cache.get_value().end();
	if (cache.get_value().begin() != cache.get_value().end()) --ite;
	for (auto it = cache.get_value().begin(); it != cache.get_value().end(); ++it) {
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
	auto cache = m_data->cached_keys.copy();
	const auto &dynamic = cache.get_raw_value();

	return kora::to_pretty_json(dynamic);
}

std::string mastermind_t::json_metabalancer_info() {
	auto cache = m_data->namespaces_states.copy();

	kora::dynamic_t raw_metabalancer_info = kora::dynamic_t::empty_object;
	auto &raw_metabalancer_info_object = raw_metabalancer_info.as_object();

	for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
		if (it->second.is_expired()) {
			continue;
		}

		const auto &ns_state = it->second.get_value();
		const auto &ns_raw_state = it->second.get_raw_value();

		raw_metabalancer_info_object[ns_state.name] = ns_raw_state.as_object()["couples"];
	}

	return kora::to_pretty_json(raw_metabalancer_info);
}

std::string mastermind_t::json_namespaces_settings() {
	auto cache = m_data->namespaces_states.copy();

	kora::dynamic_t raw_namespaces_settings = kora::dynamic_t::empty_object;
	auto &raw_namespaces_settings_object = raw_namespaces_settings.as_object();

	for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
		if (it->second.is_expired()) {
			continue;
		}

		const auto &ns_state = it->second.get_value();
		const auto &ns_raw_state = it->second.get_raw_value();

		raw_namespaces_settings_object[ns_state.name] = ns_raw_state.as_object()["settings"];
	}

	return kora::to_pretty_json(raw_namespaces_settings);
}

std::string mastermind_t::json_namespace_statistics(const std::string &ns) {
	auto cache = m_data->namespaces_states.copy();
	auto it = cache.find(ns);

	if (it == cache.end()) {
		return {};
	}

	return kora::to_pretty_json(it->second.get_raw_value().as_object()["statistics"]);
}

void
mastermind_t::set_user_settings_factory(namespace_state_t::user_settings_factory_t user_settings_factory) {
	m_data->set_user_settings_factory(std::move(user_settings_factory));
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
