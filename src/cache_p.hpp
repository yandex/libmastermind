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

#ifndef LIBMASTERMIND__SRC__CACHE_P__HPP
#define LIBMASTERMIND__SRC__CACHE_P__HPP

#include "cocaine/traits/dynamic.hpp"

#include <cocaine/framework/logging.hpp>

#include <msgpack.hpp>

#include <tuple>
#include <chrono>
#include <mutex>

namespace mastermind {

template <typename T>
class cache_base_t {
public:
	typedef T value_type;
	typedef std::shared_ptr<value_type> value_ptr_type;
	typedef std::chrono::system_clock::time_point time_point_type;
	typedef std::chrono::system_clock::duration duration_type;
	typedef std::shared_ptr<cocaine::framework::logger_t> logger_ptr_t;
	typedef std::function<value_ptr_type
		(const std::string &, const kora::dynamic_t &)> factory_t;

	template <typename... Args>
	static
	value_ptr_type
	create_value(Args &&...args) {
		return std::make_shared<value_type>(std::forward<Args>(args)...);
	}
};

template <typename T>
class synchronized_cache_map_t;

template <typename T>
class cache_t
	: public cache_base_t<T>
{
public:
	friend class synchronized_cache_map_t<T>;

	typedef cache_base_t<T> base_type;
	typedef typename base_type::value_type value_type;
	typedef typename base_type::value_ptr_type value_ptr_type;
	typedef typename base_type::time_point_type time_point_type;
	typedef typename base_type::duration_type duration_type;
	typedef typename base_type::logger_ptr_t logger_ptr_t;
	typedef typename base_type::factory_t factory_t;

	cache_t(std::string name_
			, value_ptr_type value_ = value_ptr_type()
			, kora::dynamic_t raw_value_ = kora::dynamic_t::null)
		: last_update_time(std::chrono::system_clock::now())
		, name(std::move(name_))
		, value(std::move(value_))
		, raw_value(std::move(raw_value_))
	{}

	kora::dynamic_t
	serialize() const;

	void
	deserialize(const factory_t &factory, kora::dynamic_t raw_value_);

	void
	swap(value_ptr_type &value_) {
		value.swap(value_);
		last_update_time = std::chrono::system_clock::now();
	}

	const value_ptr_type &
	get_value() const {
		return value;
	}

protected:
	time_point_type last_update_time;
	std::string name;
	value_ptr_type value;
	kora::dynamic_t raw_value;
};

template <typename T>
class synchronized_cache_t
	: public cache_t<T>
{
public:
	typedef cache_t<T> base_type;
	typedef typename base_type::value_type value_type;
	typedef typename base_type::value_ptr_type value_ptr_type;
	typedef typename base_type::time_point_type time_point_type;
	typedef typename base_type::duration_type duration_type;
	typedef typename base_type::logger_ptr_t logger_ptr_t;

	synchronized_cache_t(logger_ptr_t logger_, std::string cache_name_)
		: base_type(cache_name_)
		, logger(std::move(logger_))
		, is_expired(false)
	{}

	void
	set(value_type raw_value) {
		set(base_type::create_value(std::move(raw_value)));
	}

	void
	set(value_ptr_type value_) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		// Using swap instead of assignment allows to call value's destructor after mutex unlock
		base_type::swap(value_);
		is_expired = false;
	}

	void
	expire() {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		auto tmp_value = base_type::create_value();
		base_type::swap(tmp_value);
		is_expired = true;
	}

	value_ptr_type
	copy() const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		return base_type::value;
	}

	bool
	expire_if(const duration_type &preferable_life_time, const duration_type &warning_time
			, const duration_type &expire_time) {
		// The method is called by background thread
		// Only background thread updates last_update_time
		// That's a reason why we don't need to synchronize it

		if (is_expired) {
			return true;
		}

		auto life_time = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now() - base_type::last_update_time);

		if (expire_time <= life_time) {
			COCAINE_LOG_ERROR(logger
					, "cache \"%s\" has been expired; life-time=%ds"
					, base_type::name.c_str(), static_cast<int>(life_time.count()));
			expire();
		} else if (warning_time <= life_time) {
			COCAINE_LOG_ERROR(logger
					, "cache \"%s\" will be expired soon; life-time=%ds"
					, base_type::name.c_str(), static_cast<int>(life_time.count()));
		} else if (preferable_life_time <= life_time) {
			COCAINE_LOG_ERROR(logger
					, "cache \"%s\" is too old; life-time=%ds"
					, base_type::name.c_str(), static_cast<int>(life_time.count()));
		}

		return is_expired;
	}

protected:
	logger_ptr_t logger;
	bool is_expired;

	mutable std::mutex mutex;
};

template <typename T>
class synchronized_cache_map_t
	: public cache_base_t<T>
{
public:
	typedef cache_base_t<T> base_type;
	typedef typename base_type::value_type value_type;
	typedef typename base_type::value_ptr_type value_ptr_type;
	typedef typename base_type::time_point_type time_point_type;
	typedef typename base_type::duration_type duration_type;
	typedef typename base_type::logger_ptr_t logger_ptr_t;
	typedef typename base_type::factory_t factory_t;
	typedef cache_t<value_type> cache_type;
	typedef std::map<std::string, cache_t<T>> cache_map_t;

	synchronized_cache_map_t(logger_ptr_t logger_, std::string map_name)
		: logger(std::move(logger_))
		, name(std::move(map_name))
	{}

	void
	set(const std::string &key, cache_type cache) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		auto it = values.find(key);

		if (it != values.end()) {
			std::swap(it->second, cache);
		} else {
			values.insert(std::make_pair(key, cache));
		}
	}

	bool
	exist(const std::string &key) const {
		auto it = values.find(key);
		return it != values.end();
	}

	value_ptr_type
	copy(const std::string &key) const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		auto it = values.find(key);

		if (it != values.end()) {
			return it->second.value;
		}

		throw unknown_namespace_error();
	}

	cache_map_t
	copy() const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		return values;
	}

	bool
	expire_if(const duration_type &preferable_life_time, const duration_type &warning_time
			, const duration_type &expire_time) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		bool has_expired = false;

		{
			auto it = values.begin();
			auto end = values.end();

			auto now = std::chrono::system_clock::now();

			while (it != end) {
				const auto &key= it->first;
				const auto &cache = it->second;
				auto last_update_time = cache.last_update_time;

				auto life_time = std::chrono::duration_cast<std::chrono::seconds>(
						now - last_update_time);

				auto next_it = ++decltype(it)(it);

				if (expire_time <= life_time) {
					COCAINE_LOG_ERROR(logger
							, "cache \"%s\":\"%s\" has been expired; life-time=%ds"
							, name.c_str(), key.c_str()
							, static_cast<int>(life_time.count()));
					values.erase(it);
					has_expired = true;
				} else if (warning_time <= life_time) {
					COCAINE_LOG_ERROR(logger
							, "cache \"%s\":\"%s\" will be expired soon; life-time=%ds"
							, name.c_str(), key.c_str()
							, static_cast<int>(life_time.count()));
				} else if (preferable_life_time <= life_time) {
					COCAINE_LOG_ERROR(logger
							, "cache \"%s\":\"%s\" is too old; life-time=%ds"
							, name.c_str(), key.c_str()
							, static_cast<int>(life_time.count()));
				}

				it = next_it;
			}
		}

		return has_expired;
	}

	kora::dynamic_t
	serialize() const;

	void
	deserialize(const factory_t &factory, kora::dynamic_t raw_value);

private:
	logger_ptr_t logger;

	mutable std::mutex mutex;

	cache_map_t values;
	std::string name;
};

template <typename T>
kora::dynamic_t
cache_t<T>::serialize() const {
	static const std::string LAST_UPDATE_TIME = "last-update-time";
	static const std::string VALUE = "value";

	kora::dynamic_t result = kora::dynamic_t::empty_object;
	auto &result_object = result.as_object();

	result_object[LAST_UPDATE_TIME] = std::chrono::duration_cast<std::chrono::seconds>(
			last_update_time.time_since_epoch()).count();

	result_object[VALUE] = raw_value;

	return result;
}

template <typename T>
void
cache_t<T>::deserialize(const factory_t &factory, kora::dynamic_t raw_value_) {
	static const std::string LAST_UPDATE_TIME = "last-update-time";
	static const std::string VALUE = "value";

	raw_value = std::move(raw_value_);

	const auto &raw_value_object = raw_value.as_object();

	{
		auto seconds = raw_value_object[LAST_UPDATE_TIME].to<std::chrono::system_clock::rep>();
		last_update_time = time_point_type(std::chrono::seconds(seconds));
	}

	value = factory(name, raw_value_object[VALUE]);
}

template <typename T>
kora::dynamic_t
synchronized_cache_map_t<T>::serialize() const {
	kora::dynamic_t result = kora::dynamic_t::empty_object;
	auto &result_object = result.as_object();

	for (auto it = values.begin(), end = values.end(); it != end; ++it) {
		result_object[it->first] = it->second.serialize();
	}

	return result;
}

template <typename T>
void
synchronized_cache_map_t<T>::deserialize(const factory_t &factory, kora::dynamic_t raw_value) {
	const auto &raw_value_object = raw_value.as_object();

	for (auto it = raw_value_object.begin(), end = raw_value_object.end(); it != end; ++it) {
		const auto &name = it->first;
		cache_t<T> cache(name, factory(name, it->second), it->second);

		auto insert_result = values.insert(std::make_pair(name, cache));
		if (std::get<1>(insert_result) == false) {
			std::get<0>(insert_result)->second = std::move(cache);
		}
	}
}

} // namespace mastermind

namespace msgpack {

template <typename T>
mastermind::cache_t<T> &
operator >> (msgpack::object object, mastermind::cache_t<T> &cache) {
	cache.deserialize(object);
	return cache;
}

template <typename T, typename Stream>
msgpack::packer<Stream>
operator << (msgpack::packer<Stream> &packer, const mastermind::cache_t<T> &cache) {
	cache.serialize(packer);
	return packer;
}

template <typename T>
mastermind::synchronized_cache_t<T> &
operator >> (msgpack::object object, mastermind::synchronized_cache_t<T> &cache) {
	cache.deserialize(object);
	return cache;
}

template <typename T, typename Stream>
msgpack::packer<Stream>
operator << (msgpack::packer<Stream> &packer, const mastermind::synchronized_cache_t<T> &cache) {
	cache.serialize(packer);
	return packer;
}

template <typename T>
mastermind::synchronized_cache_map_t<T> &
operator >> (msgpack::object object, mastermind::synchronized_cache_map_t<T> &cache) {
	cache.deserialize(object);
	return cache;
}

template <typename T, typename Stream>
msgpack::packer<Stream>
operator << (msgpack::packer<Stream> &packer, const mastermind::synchronized_cache_map_t<T> &cache) {
	cache.serialize(packer);
	return packer;
}

} // namespace msgpack
#endif /* LIBMASTERMIND__SRC__CACHE_P__HPP */

