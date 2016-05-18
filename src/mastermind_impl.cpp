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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mastermind_impl.hpp"
#include "namespace_p.hpp"

namespace {

std::ostream& operator<<(std::ostream &ostream, const mastermind::mastermind_t::remote_t &remote) {
	if (remote.first.empty()) {
		ostream << "none";
	} else {
		ostream << remote.first << ':' << remote.second;
	}
	return ostream;
}

}

namespace mastermind {

mastermind_t::data::data(
		const remotes_t &remotes,
		const std::shared_ptr<blackhole::logger_t> &logger,
		int group_info_update_period,
		std::string cache_path,
		int warning_time_,
		int expire_time_,
		std::string worker_name,
		int enqueue_timeout_,
		int reconnect_timeout_,
		namespace_state_t::user_settings_factory_t user_settings_factory_,
		bool auto_start
		)
	: m_logger(logger)
	, m_remotes(remotes)
	, m_next_remote(0)
	, m_cache_path(std::move(cache_path))
	, m_worker_name(std::move(worker_name))
	, cached_keys({{}, kora::dynamic_t::empty_object, "cached_keys"})
	, elliptics_remotes({std::vector<std::string>(), kora::dynamic_t::empty_array
			, "elliptics_remotes"})
	, namespaces_settings({"namespaces_settings"})
	, bad_groups({"bad_groups"})
	, fake_groups_info({"fake_groups_info"})
	, m_group_info_update_period(group_info_update_period)
	, m_done(false)
	, warning_time(std::chrono::seconds(warning_time_))
	, expire_time(std::chrono::seconds(expire_time_))
	, enqueue_timeout(enqueue_timeout_)
	, reconnect_timeout(reconnect_timeout_)
	, user_settings_factory(std::move(user_settings_factory_))
	, cache_is_expired(false)
{
	if (remotes.empty()) {
		throw remotes_empty_error();
	}

	if (auto_start) {
		start();
	}
}

mastermind_t::data::~data() {
}

std::shared_ptr<namespace_state_init_t::data_t>
mastermind_t::data::get_namespace_state(const std::string &name) const {
	try {
		auto ns_state_cache = namespaces_states.copy(name);
		auto result = ns_state_cache.get_shared_value();

		if (user_settings_factory && !result->settings.user_settings_ptr) {
			throw std::runtime_error("user settings were not initialized");
		}

		return result;

	} catch (const std::exception &e) {
		MM_LOG_INFO(m_logger, "libmastermind: {}: cannot obtain namespace_state for {}: {}", __func__, name, e.what());
	}

	return std::shared_ptr<namespace_state_init_t::data_t>();
}

void
mastermind_t::data::start() {
	if (is_running()) {
		throw update_loop_already_started();
	}

	deserialize();

	m_done = false;
	m_weight_cache_update_thread = std::thread(std::bind(
				&mastermind_t::data::collect_info_loop, this));
}

void
mastermind_t::data::stop() {
	if (!is_running()) {
		throw update_loop_already_stopped();
	}

	try {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			(void)lock;
			m_done = true;
			m_weight_cache_condition_variable.notify_one();
		}
		m_weight_cache_update_thread.join();
	} catch (const std::system_error &) {
	}
	m_app.reset();
	m_service_manager.reset();
}

bool
mastermind_t::data::is_running() const {
	return std::thread::id() != m_weight_cache_update_thread.get_id();
}

bool
mastermind_t::data::is_valid() const {
	auto cache_map = namespaces_states.copy();
	size_t important_namespaces = 0;

	for (auto it = cache_map.begin(), end = cache_map.end(); it != end; ++it) {
		const auto &cache = it->second;

		if (user_settings_factory && !cache.get_value_unsafe().settings.user_settings_ptr) {
			// That means proxy is not interested in this namespace
			continue;
		}

		++important_namespaces;

		if (cache.is_expired()) {
			return false;
		}
	}

	if (important_namespaces == 0) {
		return false;
	}

	return true;
}

void mastermind_t::data::reconnect() {
	std::lock_guard<std::mutex> lock(m_reconnect_mutex);
	(void) lock;

	size_t end = m_next_remote;
	size_t index = m_next_remote;
	size_t size = m_remotes.size();

	do {
		auto &remote = m_remotes[index];
		try {
			using cocaine::framework::service_manager_t;

			MM_LOG_INFO(m_logger, "libmastermind: {}: try to connect to locator {}", __func__, remote);

			//FIXME: make manager's thread count configurable?
			auto manager = std::unique_ptr<service_manager_t>(new service_manager_t({remote}, 1));

			//XXX: service_manager_t does not try to connect to locators on construction,
			// so this message is not actually correct
			MM_LOG_INFO(m_logger, "libmastermind: {}: connected to locator, getting mastermind service", __func__);

			auto mastermind = manager->create<cocaine::io::app_tag>(m_worker_name);

			{
				auto g = mastermind.connect();
				g.wait_for(reconnect_timeout);
				if (g.ready()) {
					// throws exception in case of connection error
					g.get();

				} else {
					MM_LOG_ERROR(m_logger, "libmastermind: {}: cannot get mastermind-service in {} milliseconds from {}",
						__func__,
						static_cast<int>(reconnect_timeout.count()),
						remote
					);
					index = (index + 1) % size;
					continue;
				}
			}

			MM_LOG_INFO(m_logger, "libmastermind: {}: connected to mastermind via locator {}", __func__, remote);

			m_current_remote = remote;
			m_next_remote = (index + 1) % size;

			m_app.reset(new cocaine::framework::service<cocaine::io::app_tag>(std::move(mastermind)));
			m_service_manager = std::move(manager);
			return;

		} catch (const cocaine::framework::error_t &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: framework error: {}, {}; host: {}", __func__, e.code(), e.code().message(), remote);
		} catch (const std::system_error &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: system error: {}, {}; host: {}", __func__, e.code(), e.code().message(), remote);
		} catch (const std::exception &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: unexpected error: {}; host: {}", __func__, e.what(), remote);
		}

		index = (index + 1) % size;

	} while (index != end);

	m_current_remote = remote_t();
	m_app.reset();
	m_service_manager.reset();

	MM_LOG_ERROR(m_logger, "libmastermind: {}: cannot reconnect to any host, stopping", __func__);

	throw std::runtime_error("reconnect error");
}

kora::dynamic_t
mastermind_t::data::enqueue(const std::string &event) {
	return enqueue(event, kora::dynamic_t::empty_string);
}

kora::dynamic_t
mastermind_t::data::enqueue_gzip(const std::string &event) {
	kora::dynamic_t args = kora::dynamic_t::empty_object;
	args.as_object()["gzip"] = true;

	return enqueue(event, std::move(args));
}

kora::dynamic_t
mastermind_t::data::enqueue(const std::string &event, kora::dynamic_t args) {
	auto need_ungzip = [&]() {
		if (!args.is_object()) {
			return false;
		}

		auto it = args.as_object().find("gzip");

		if (args.as_object().end() != it) {
			if (it->second.to<bool>()) {
				return true;
			}
		}

		return false;
	}();

	// cocaine protocol is used as a mere transport,
	// so explicit packing (and unpacking) is required

	auto raw_args = [&]() -> std::string {
		std::ostringstream packed;
		{
			msgpack::packer<std::ostringstream> packer(packed);
			cocaine::io::type_traits<kora::dynamic_t>::pack(packer, args);
		}
		return std::move(packed.str());
	}();

	auto raw_result = enqueue_with_reconnect(event, raw_args);

	auto result = [&]() -> kora::dynamic_t {
		msgpack::unpacked unpacked;
		msgpack::unpack(&unpacked, raw_result.data(), raw_result.size());
		auto object = unpacked.get();

		if (need_ungzip) {
			std::string gzip_result;
			cocaine::io::type_traits<std::string>::unpack(object, gzip_result);

			auto ungzip_result = ungzip(gzip_result);
			std::istringstream iss(ungzip_result);
			return kora::dynamic::read_json(iss);
		} else {
			kora::dynamic_t result;
			cocaine::io::type_traits<kora::dynamic_t>::unpack(object, result);
			return result;
		}
	}();

	return result;
}

void
mastermind_t::data::collect_namespaces_states() {
	try {
		auto dynamic_namespaces_states = enqueue_gzip("get_namespaces_states").as_object();

		for (auto it = dynamic_namespaces_states.begin(), end = dynamic_namespaces_states.end();
				it != end; ++it) {
			const auto &name = it->first;

			try {
				if (namespace_state_is_deleted(it->second)) {
					bool removed = namespaces_states.remove(name);
					MM_LOG_INFO(m_logger, "libmastermind: namespace \"{}\" was detected as deleted {}",
						name,
						(removed ? "and was removed from the cache" : "but it is already not in the cache")
					);
					continue;
				}

				auto ns_state = create_namespaces_states(name, it->second);

				// TODO: check new ns_state is better than the old one
				// auto old_ns_state = namespaces_states.copy(name);
				// if (ns_state is better than old_ns_state) {
				// 	namespaces_states.set(name, ns_state);
				// } else {
				// 	throw std::runtime_error("old namespace_state is better than the new one");
				// }
				namespaces_states.set(name, {std::move(ns_state), std::move(it->second), name});

			} catch (const std::exception &e) {
				MM_LOG_ERROR(m_logger, "libmastermind: cannot update namespace_state for {}: {}", name, e.what());
			}
		}

	} catch (const std::exception &e) {
		MM_LOG_ERROR(m_logger, "libmastermind: {}: {}", __func__, e.what());
	}
}

bool mastermind_t::data::collect_cached_keys() {
	try {
		auto raw = enqueue_gzip("get_cached_keys");
		auto cache = create_cached_keys("", raw);

		cached_keys.set({std::move(cache), std::move(raw)});

		return true;

	} catch(const std::exception &e) {
		MM_LOG_ERROR(m_logger, "libmastermind: {}: {}", __func__, e.what());
	}
	return false;
}

bool mastermind_t::data::collect_elliptics_remotes() {
	try {
		auto raw_elliptics_remotes = enqueue("get_config_remotes");
		auto cache = create_elliptics_remotes("", raw_elliptics_remotes);
		elliptics_remotes.set({std::move(cache), std::move(raw_elliptics_remotes)});
		return true;

	} catch (const std::exception &e) {
		MM_LOG_ERROR(m_logger, "libmastermind: {}: {}", __func__, e.what());
	}
	return false;
}

void mastermind_t::data::collect_info_loop_impl() {
	MM_LOG_INFO(m_logger, "libmastermind: collect_info_loop: begin; current host: {}", m_current_remote);

	auto start_time = std::chrono::system_clock::now();

	{
		spent_time_printer_t helper("collect_namespaces_states", m_logger);
		collect_namespaces_states();
	}
	{
		spent_time_printer_t helper("collect_cached_keys", m_logger);
		collect_cached_keys();
	}
	{
		spent_time_printer_t helper("collect_elliptics_remotes", m_logger);
		collect_elliptics_remotes();
	}

	cache_expire();
	generate_fake_caches();
	serialize();

	auto elapsed = std::chrono::system_clock::now() - start_time;

	MM_LOG_INFO(m_logger, "libmastermind: collect_info_loop: end; current host: {}; spent time: {} milliseconds",
		m_current_remote,
		std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
	);
}

void mastermind_t::data::collect_info_loop() {
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_done) {
		MM_LOG_INFO(m_logger, "libmastermind: have to stop immediately");
		return;
	}

	try {
		reconnect();

	} catch (const std::runtime_error &e) {
		// failed to connect
	} catch (const std::exception &e) {
		MM_LOG_ERROR(m_logger, "libmastermind: collect_info_loop: unexpected error: {}", e.what());
	}

#if __GNUC_MINOR__ >= 6
	auto no_timeout = std::cv_status::no_timeout;
	auto timeout = std::cv_status::timeout;
	auto tm = timeout;
#else
	bool no_timeout = true;
	bool timeout = false;
	bool tm = timeout;
#endif
	MM_LOG_INFO(m_logger, "libmastermind: collect_info_loop: update period is {}", m_group_info_update_period);
	const auto update_period = std::chrono::seconds(m_group_info_update_period);
	do {
		collect_info_loop_impl();

		process_callbacks();

		tm = timeout;
		do {
			tm = m_weight_cache_condition_variable.wait_for(lock, update_period);
		} while(tm == no_timeout && m_done == false);
	} while(m_done == false);
}

void
mastermind_t::data::cache_expire() {
	auto preferable_life_time = std::chrono::seconds(m_group_info_update_period);

	cache_is_expired = false;

	{
		auto cache = cached_keys.copy();
		if (!cache.is_expired() && check_cache_for_expire("cached_keys", cache
					, preferable_life_time, warning_time, expire_time)) {
			cache.set_expire(true);
			cached_keys.set(cache);
		}
	}

	{
		auto cache = elliptics_remotes.copy();
		if (!cache.is_expired() && check_cache_for_expire("elliptics_remotes", cache
					, preferable_life_time, warning_time, expire_time)) {
			cache.set_expire(true);
			elliptics_remotes.set(cache);
		}
	}

	{
		auto cache_map = namespaces_states.copy();

		for (auto it = cache_map.begin(), end = cache_map.end(); it != end; ++it) {
			const auto &name = it->first;
			auto &cache = it->second;

			cache_is_expired = cache_is_expired || cache.is_expired();

			if (check_cache_for_expire("namespaces_states:" + name
						, cache, preferable_life_time, warning_time, expire_time)) {
				cache_is_expired = true;

				if (!cache.is_expired()) {
					cache.set_expire(true);
					namespaces_states.set(name, std::move(cache));
				}
			}
		}
	}
}

void
mastermind_t::data::generate_fake_caches() {
	std::vector<groups_t> raw_bad_groups;
	std::map<group_t, fake_group_info_t> raw_fake_groups_info;
	std::vector<namespace_settings_t> raw_namespaces_settings;

	auto cache = namespaces_states.copy();

	for (auto ns_it = cache.begin(), ns_end = cache.end(); ns_it != ns_end; ++ns_it) {
		if (ns_it->second.is_expired()) {
			continue;
		}

		const auto &states = ns_it->second.get_value();
		const auto &raw_states = ns_it->second.get_raw_value();
		const auto &couples = states.couples.couple_info_map;


		for (auto cim_it = couples.begin(), cim_end = couples.end();
				cim_it != cim_end; ++cim_it) {
			const auto &couple = cim_it->second;
			for (auto it = couple.groups_info_map_iterator.begin()
					, end = couple.groups_info_map_iterator.end();
					it != end; ++it) {
				fake_group_info_t fake_group_info;

				fake_group_info.id = (*it)->second.id;
				fake_group_info.groups = couple.groups;
				fake_group_info.free_effective_space = couple.free_effective_space;
				fake_group_info.ns = states.name;
				fake_group_info.group_status = (*it)->second.status;

				raw_fake_groups_info.insert(std::make_pair(fake_group_info.id, fake_group_info));
			}

			if (couple.status == namespace_state_init_t::data_t::couples_t
					::couple_info_t::status_tag::BAD) {
				raw_bad_groups.emplace_back(couple.groups);
			}
		}

		raw_namespaces_settings.emplace_back(create_namespace_settings(states.name
					, raw_states.as_object()["settings"]));
	}

	bad_groups.set({std::move(raw_bad_groups)});
	fake_groups_info.set({std::move(raw_fake_groups_info)});
	namespaces_settings.set({std::move(raw_namespaces_settings)});
}

void mastermind_t::data::serialize() {
	kora::dynamic_t raw_cache = kora::dynamic_t::empty_object;
	auto &raw_cache_object = raw_cache.as_object();

	// TODO: remove ridiculous macros
#define PACK_CACHE(cache) \
	do { \
		raw_cache_object[#cache] = cache.copy().serialize(); \
	} while (false)

	PACK_CACHE(cached_keys);
	PACK_CACHE(elliptics_remotes);

#undef PACK_CACHE

	{
		auto cache = namespaces_states.copy();
		kora::dynamic_t raw_namespaces_states = kora::dynamic_t::empty_object;
		auto &raw_namespaces_states_object = raw_namespaces_states.as_object();

		for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
			raw_namespaces_states_object[it->first] = it->second.serialize();
		}

		raw_cache_object["namespaces_states"] = raw_namespaces_states;
	}

	msgpack::sbuffer sbuffer;
	msgpack::packer<msgpack::sbuffer> packer(sbuffer);

	cocaine::io::type_traits<kora::dynamic_t>::pack(packer, raw_cache);

	{
		std::ofstream output(m_cache_path.c_str());
		std::copy(sbuffer.data(), sbuffer.data() + sbuffer.size(), std::ostreambuf_iterator<char>(output));
	}

	MM_LOG_INFO(m_logger, "libmastermind: {}: cache saved to '{}'", __func__, m_cache_path);
}

bool
mastermind_t::data::namespace_state_is_deleted(const kora::dynamic_t &raw_value) {
	const auto &state_object = raw_value.as_object();
	auto it_settings = state_object.find("settings");

	if (it_settings == state_object.end()) {
		return false;
	}

	const auto &settings_object = it_settings->second.as_object();
	auto it_service = settings_object.find("__service");

	if (it_service == settings_object.end()) {
		return false;
	}

	const auto &service_object = it_service->second.as_object();
	auto it_is_deleted = service_object.find("is_deleted");

	if (it_is_deleted == service_object.end()) {
		return false;
	}

	const auto &is_deleted_object = it_is_deleted->second;
	return is_deleted_object.to<bool>();
}

namespace_state_init_t::data_t
mastermind_t::data::create_namespaces_states(const std::string &name
		, const kora::dynamic_t &raw_value) {
	namespace_state_init_t::data_t ns_state{name
		, kora::config_t(name, raw_value), user_settings_factory};
	MM_LOG_INFO(m_logger, "libmastermind: namespace_state: {}", ns_state.extract);
	return ns_state;
}

mastermind::cached_keys_t
mastermind_t::data::create_cached_keys(const std::string &name
		, const kora::dynamic_t &raw_value) {
	(void) name;
	return {raw_value};
}

std::vector<std::string>
mastermind_t::data::create_elliptics_remotes(const std::string &name
		, const kora::dynamic_t &raw_value) {
	(void) name;

	std::vector<std::string> result;

	const auto &raw_value_array = raw_value.as_array();

	for (auto p_it = raw_value_array.begin(), p_end = raw_value_array.end();
			p_it != p_end; ++p_it) {
		const auto &tuple = p_it->as_array();
		const auto &name = tuple[0].to<std::string>();
		const auto &port = tuple[1].to<int>();
		const auto &family = tuple[2].to<int>();

		std::ostringstream oss;
		oss << name << ':' << port << ':' << family;

		result.emplace_back(oss.str());
	}

	if (result.empty()) {
		throw std::runtime_error("elliptics-remotes list is empty");
	}

	return result;
}

namespace_settings_t
mastermind_t::data::create_namespace_settings(const std::string &name
		, const kora::dynamic_t &raw_value) {
	namespace_settings_t::data item;

	kora::config_t config(name, raw_value);

	item.name = name;

	item.groups_count = config.at<int>("groups-count");
	item.success_copies_num = config.at<std::string>("success-copies-num");

	item.auth_key = config.at<std::string>("auth-key", "");

	if (config.has("auth-keys")) {
		const auto &auth_keys_config = config.at("auth-keys");
		item.auth_key_for_write = auth_keys_config.at<std::string>("write", "");
		item.auth_key_for_read = auth_keys_config.at<std::string>("read", "");
	}

	if (config.has("static-couple")) {
		const auto &static_couple_config = config.at("static-couple");

		for (size_t index = 0, size = static_couple_config.size(); index != size; ++index) {
			item.static_couple.emplace_back(static_couple_config.at<int>(index));
		}
	}

	if (config.has("signature")) {
		const auto &signature_config = config.at("signature");
		item.sign_token = signature_config.at<std::string>("token", "");
		item.sign_path_prefix = signature_config.at<std::string>("path_prefix", "");
		item.sign_port = signature_config.at<std::string>("port", "");
	}

	if (config.has("redirect")) {
		const auto &redirect_config = config.at("redirect");
		item.redirect_expire_time = redirect_config.at<int>("expire-time", 0);
		item.redirect_content_length_threshold
			= redirect_config.at<int>("content-length-threshold", -1);
	}

	item.is_active = config.at<bool>("is_active", false);

	if (config.has("features")) {
		const auto &features_config = config.at("features");

		item.can_choose_couple_to_upload
			= features_config.at<bool>("select-couple-to-upload", false);

		if (features_config.has("multipart")) {
			const auto &multipart_features_config = features_config.at("multipart");

			item.multipart_content_length_threshold
				= multipart_features_config.at<int64_t>("content-length-threshold", 0);
		}
	}

	return { std::move(item) };
}

void mastermind_t::data::deserialize() {
	std::string file;
	{
		std::ifstream input(m_cache_path.c_str());
		if (input.is_open() == false) {
			return;
		}
		typedef std::istreambuf_iterator<char> it_t;
		file.assign(it_t(input), it_t());
	}

	try {
		msgpack::unpacked msg;
		msgpack::unpack(&msg, file.data(), file.size());
		msgpack::object object = msg.get();

		kora::dynamic_t raw_cache;

		cocaine::io::type_traits<kora::dynamic_t>::unpack(object, raw_cache);

		auto &raw_cache_object = raw_cache.as_object();

#define TRY_UNPACK_CACHE(cache) \
		do { \
			try { \
				cache.set(cache##_t::cache_type(raw_cache_object[#cache].as_object() \
							, std::bind(&data::create_##cache, this \
								, std::placeholders::_1, std::placeholders::_2) \
							, #cache)); \
			} catch (const std::exception &e) { \
				MM_LOG_ERROR(m_logger, "libmastermind: {}: cannot deserialize {}: {}", __func__, #cache, e.what()); \
			} \
		} while (false)

		try {
			cached_keys.set(synchronized_cache_t<cached_keys_t>::cache_type(
						raw_cache_object["cached_keys"].as_object()
						, std::bind(&data::create_cached_keys, this
							, std::placeholders::_1, std::placeholders::_2)
						, "cached_keys"));
		} catch (const std::exception &e) {
			MM_LOG_ERROR(m_logger, "libmastermind: {}: cannot deserialize cached_keys: {}", __func__, e.what());
		}

		TRY_UNPACK_CACHE(elliptics_remotes);

#undef TRY_UNPACK_CACHE

		{
			const auto &raw_namespaces_states = raw_cache_object["namespaces_states"];
			const auto &raw_namespaces_states_object = raw_namespaces_states.as_object();

			for (auto it = raw_namespaces_states_object.begin()
					, end = raw_namespaces_states_object.end();
					it != end; ++it) {
				const auto &name = it->first;

				try {
					namespaces_states.set(name, namespaces_states_t::cache_type(
								it->second.as_object()
								, std::bind(&data::create_namespaces_states, this
									, std::placeholders::_1, std::placeholders::_2)
								, name));
				} catch (const std::exception &e) {
					MM_LOG_ERROR(m_logger, "libmastermind: {}: cannot update namespace_state for {}: {}", name, e.what());
				}
			}
		}

		cache_expire();
		generate_fake_caches();

		MM_LOG_INFO(m_logger, "libmastermind: {}: cache restored from '{}'", __func__, m_cache_path);

	} catch (const std::exception &e) {
		MM_LOG_WARNING(m_logger, "libmastermind: {}: cannot restore cache from '{}': {}", __func__, m_cache_path, e.what());
	} catch (...) {
		MM_LOG_WARNING(m_logger, "libmastermind: {}: cannot restore cache from '{}': unknown error", __func__, m_cache_path);
	}
}

void
mastermind_t::data::set_user_settings_factory(namespace_state_t::user_settings_factory_t user_settings_factory_) {
	// TODO: forbid to change settings during background thread is running
	std::lock_guard<std::mutex> lock_guard(m_mutex);

	user_settings_factory = std::move(user_settings_factory_);
}

void mastermind_t::data::cache_force_update() {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;
	collect_info_loop_impl();
	process_callbacks();
}

void mastermind_t::data::set_update_cache_callback(const std::function<void (void)> &callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;

	m_cache_update_callback = callback;
}

void
mastermind_t::data::set_update_cache_ext1_callback(const std::function<void (bool)> &callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	(void) lock;

	cache_update_ext1_callback = callback;
}

void
mastermind_t::data::process_callbacks() {
	if (m_cache_update_callback) {
		m_cache_update_callback();
	}

	if (cache_update_ext1_callback) {
		cache_update_ext1_callback(cache_is_expired);
	}
}

} // namespace mastermind
