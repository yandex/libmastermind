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
#include "utils.hpp"

#include <cocaine/framework/logging.hpp>

#include <msgpack.hpp>

#include <tuple>
#include <chrono>
#include <mutex>

namespace mastermind {

template <typename T>
class cache_t
{
public:
	typedef T value_type;
	typedef std::tuple<value_type, kora::dynamic_t> tuple_value_type;
	typedef std::shared_ptr<tuple_value_type> shared_value_type;

	typedef std::function<value_type
		(const std::string &, const kora::dynamic_t &)> factory_t;

	cache_t(std::string name_ = "")
		: last_update_time(clock_type::now())
		, m_is_expired(false)
		, name(std::move(name_))
		, shared_value(std::make_shared<tuple_value_type>(value_type(), kora::dynamic_t::null))
	{
	}

	cache_t(value_type value_, std::string name_ = "")
		: last_update_time(clock_type::now())
		, m_is_expired(false)
		, name(std::move(name_))
		, shared_value(std::make_shared<tuple_value_type>(std::move(value_)
					, kora::dynamic_t::null))
	{
	}

	cache_t(value_type value_, kora::dynamic_t raw_value_, std::string name_ = "")
		: last_update_time(clock_type::now())
		, m_is_expired(false)
		, name(std::move(name_))
		, shared_value(std::make_shared<tuple_value_type>(std::move(value_)
					, std::move(raw_value_)))
	{
	}

	cache_t(kora::dynamic_t raw_value_, const factory_t &factory, std::string name_ = "")
		: last_update_time(clock_type::now())
		, m_is_expired(false)
		, name(std::move(name_))
	{
		static const std::string LAST_UPDATE_TIME = "last-update-time";
		static const std::string VALUE = "value";

		const auto &raw_value_object = raw_value_.as_object();

		{
			auto seconds = raw_value_object[LAST_UPDATE_TIME].to<clock_type::rep>();
			last_update_time = time_point_type(std::chrono::seconds(seconds));
		}

		const auto &raw_value = raw_value_object[VALUE];
		shared_value = std::make_shared<tuple_value_type>(factory(name, raw_value), raw_value);
	}

	kora::dynamic_t
	serialize() const {
		static const std::string LAST_UPDATE_TIME = "last-update-time";
		static const std::string VALUE = "value";

		kora::dynamic_t result = kora::dynamic_t::empty_object;
		auto &result_object = result.as_object();

		result_object[LAST_UPDATE_TIME] = std::chrono::duration_cast<std::chrono::seconds>(
				last_update_time.time_since_epoch()).count();

		result_object[VALUE] = get_raw_value();

		return result;
	}

	time_point_type
	get_last_update_time() const {
		return last_update_time;
	}

	bool
	is_expired() const {
		return m_is_expired;
	}

	void
	set_expire(bool is_expired_) {
		m_is_expired = is_expired_;
	}

	const std::string &
	get_name() const {
		return name;
	}

	const value_type &
	get_value() const {
		if (is_expired()) {
			throw cache_is_expired_error();
		}

		return std::get<0>(*shared_value);
	}

	const kora::dynamic_t &
	get_raw_value() const {
		return std::get<1>(*shared_value);
	}

private:
	time_point_type last_update_time;
	bool m_is_expired;

	std::string name;
	shared_value_type shared_value;
};

template <typename T>
class synchronized_cache_t
{
public:
	typedef cache_t<T> cache_type;

	synchronized_cache_t(cache_type cache_)
		: cache(std::move(cache_))
	{
	}

	void
	set(cache_type cache_) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		cache = std::move(cache_);
	}

	cache_type
	copy() const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		return cache;
	}

private:
	mutable std::mutex mutex;
	cache_type cache;
};

template <typename T>
class synchronized_cache_map_t
{
public:
	typedef cache_t<T> cache_type;
	typedef std::map<std::string, cache_type> cache_map_t;

	synchronized_cache_map_t()
	{
	}

	void
	set(const std::string &key, cache_type cache) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		auto insert_result = cache_map.insert(std::make_pair(key, cache));
		if (!std::get<1>(insert_result)) {
			std::get<0>(insert_result)->second = std::move(cache);
		}
	}

	cache_map_t
	copy() const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		return cache_map;
	}

	cache_type
	copy(const std::string &key) const {
		std::lock_guard<std::mutex> lock_guard(mutex);
		(void) lock_guard;

		auto it = cache_map.find(key);

		if (it == cache_map.end()) {
			throw unknown_namespace_error();
		}

		return it->second;
	}

private:
	mutable std::mutex mutex;
	cache_map_t cache_map;
};

} // namespace mastermind

#endif /* LIBMASTERMIND__SRC__CACHE_P__HPP */

