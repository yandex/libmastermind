/*
	Client library for mastermind
	Copyright (C) 2013-2015 Yandex

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

#include <memory>
#include <iostream>
#include <cstdlib>
#include <limits>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/tokenizer.hpp>

#include <boost/thread/tss.hpp>
#include <blackhole/attribute.hpp>
#include <blackhole/logger.hpp>
#include <blackhole/record.hpp>
#include <blackhole/formatter.hpp>
#include <blackhole/formatter/string.hpp>
#include <blackhole/scope/watcher.hpp>
#include <blackhole/scope/manager.hpp>
#include <blackhole/extensions/writer.hpp>

#include "mastermind-cache/mastermind.hpp"

namespace bp = boost::python;
namespace mm = mastermind;

namespace mastermind {
namespace binding {

class gil_guard_t {
public:
	gil_guard_t()
		: gstate(PyGILState_Ensure())
		, is_released(false)
	{
	}

	~gil_guard_t() {
		release();
	}

	gil_guard_t(const gil_guard_t &) = delete;

	gil_guard_t(gil_guard_t &&) = delete;

	gil_guard_t &
	operator = (const gil_guard_t &) = delete;

	gil_guard_t &
	operator = (gil_guard_t &&) = delete;

	void
	release() {
		if (is_released) {
			return;
		}

		PyGILState_Release(gstate);
		is_released = true;
	}

private:
	PyGILState_STATE gstate;
	bool is_released;
};

class py_allow_threads_scoped
{
public:
	py_allow_threads_scoped()
	: save(PyEval_SaveThread())
	{}

	void disallow()
	{
		PyEval_RestoreThread(save);
		save = NULL;
	}

	~py_allow_threads_scoped()
	{
		if (save)
			PyEval_RestoreThread(save);
	}
private:
	PyThreadState* save;
};

namespace detail {

bp::object
convert(const kora::dynamic_t &d, const gil_guard_t &gil_guard);

bp::object
convert(const kora::dynamic_t::array_t &v, const gil_guard_t &gil_guard) {
	bp::list result;

	for (auto it = v.begin(), end = v.end(); it != end; ++it) {
		result.append(convert(*it, gil_guard));
	}

	return result;
}

bp::object
convert(const kora::dynamic_t::object_t &v, const gil_guard_t &gil_guard) {
	bp::dict result;

	for (auto it = v.begin(), end = v.end(); it != end; ++it) {
		result[it->first] = convert(it->second, gil_guard);
	}

	return result;
}

bp::object
convert(const kora::dynamic_t &d, const gil_guard_t &gil_guard) {
	if (d.is_null()) {
		return bp::object{};
	} else if (d.is_bool()) {
		return bp::object{d.as_bool()};
	} else if (d.is_int()) {
		return bp::object{d.as_int()};
	} else if (d.is_uint()) {
		return bp::object{d.as_uint()};
	} else if (d.is_double()) {
		return bp::object{d.as_double()};
	} else if (d.is_string()) {
		return bp::object{d.as_string()};
	} else if (d.is_array()) {
		return convert(d.as_array(), gil_guard);
	} else if (d.is_object()) {
		return convert(d.as_object(), gil_guard);
	}

	return bp::object{};
}

} // namespace detail

namespace {

using namespace blackhole;

/// `python_logger_wrapper` wraps python logger into blackhole-v1.0 logger interface.
///

/// Scope manager.
//
//XXX: pristine copy from blackhole/root.cpp,
// We're forced to use it because blackhole requires manager_t to remember
// sequence of watcher_t object, dummy implementation without memory
// will break assertion in ~watcher_t()
//
class thread_manager_t : public scope::manager_t {
	boost::thread_specific_ptr<scope::watcher_t> inner;

public:
	thread_manager_t() : inner([](scope::watcher_t*) {}) {}

	auto get() const -> scope::watcher_t* {
		return inner.get();
	}

	auto reset(scope::watcher_t* value) -> void {
		inner.reset(value);
	}
};

class python_logger_wrapper : public logger_t
{
	bp::object python_logger;
	thread_manager_t scope_manager;

	// Assuming logger configuration will not be changing at runtime,
	// cache
	int effective_level;
	bool disabled;

	// See logging.hpp for our definition of severity values.
	// See https://docs.python.org/2/library/logging.html#levels
	// for predefined python logging levels.
	// It is possible to change those python loglevels into something
	// different or completely new, but that ability is very rarely used.
	// Here we assume to be working with standard python level names
	// and numeric values.
	auto map_severity_to_python_loglevel(severity_t severity) -> int {
		return (severity + 1) * 10;
	}

public:
	python_logger_wrapper(const gil_guard_t &)
		: python_logger(bp::import("logging").attr("getLogger")("mastermind_cache"))
		, effective_level(bp::extract<int>(python_logger.attr("getEffectiveLevel")()))
		, disabled(bp::extract<bool>(python_logger.attr("disabled")))
	{}

	/// blackhole::logger_t interface

	virtual ~python_logger_wrapper() = default;

	/// Logs the given message with the specified severity level.
	virtual auto log(severity_t severity, const message_t& message) -> void {
		attribute_pack pack;
		log(severity, {message, [&message]() { return message; }}, pack);
	}

	/// Logs the given message with the specified severity level and attributes pack attached.
	virtual auto log(severity_t severity, const message_t& message, attribute_pack& pack) -> void {
		log(severity, {message, [&message]() { return message; }}, pack);
	}

	/// Logs a message which is only to be constructed if the result record passes filtering with
	/// the specified severity and including the attributes pack provided.
	virtual auto log(severity_t severity, const lazy_message_t& message, attribute_pack& pack) -> void {
		const int level = map_severity_to_python_loglevel(severity);

		// skip all processing if its not needed
		if (!disabled && level < effective_level) {
			return;
		}

		//XXX: not taking into account the possibility of filters

		if (scope_manager.get()) {
			scope_manager.get()->collect(pack);
		}

		auto instantiated = message.supplier();
		record_t record(severity, instantiated, pack);
		writer_t writer;
		if (pack.empty()) {
			formatter::string_t("{message}").format(record, writer);
		} else {
			formatter::string_t("{message}, attrs: [{...}]").format(record, writer);
		}

		auto formatted = writer.result().to_string();
		gil_guard_t gil_guard;
		python_logger.attr("log")(level, formatted);
	}

	/// Returns a scoped attributes manager reference.
	///
	/// Returned manager allows the external tools to attach scoped attributes to the current logger
	/// instance, making every further log event to contain them until the registered scoped guard
	/// keeped alive.
	///
	/// \returns a scoped attributes manager.
	virtual auto manager() -> scope::manager_t& {
		return scope_manager;
	}
};

} // anonymous namespace

class namespace_state_t {
public:
	class user_settings_t : public mm::namespace_state_t::user_settings_t {
	public:
		user_settings_t(bp::object dict_, const gil_guard_t &)
			: dict(new bp::object{std::move(dict_)})
		{
		}

		~user_settings_t() {
			gil_guard_t gil_guard;
			dict.reset();
		}

		std::unique_ptr<bp::object> dict;
	};

	class couples_t {
	public:
		couples_t(mm::namespace_state_t impl_)
			: impl(std::move(impl_))
		{}

		bp::list
		get_couple_read_preference(int group) const {
			auto pref = impl.couples().get_couple_read_preference(group);

			bp::list result;

			for (auto it = pref.begin(), end = pref.end(); it != end; ++it) {
				result.append(*it);
			}

			return result;
		}

		bp::dict
		get_couple_groupset(int group, const std::string &groupset_id) const {
			auto groupset = impl.couples().get_couple_groupset(group, groupset_id);

			bp::dict result;

			result["free_effective_space"] = groupset.free_effective_space();
			result["free_reserved_space"] = groupset.free_reserved_space();

			result["type"] = groupset.type();
			result["status"] = groupset.status();
			result["id"] = groupset.id();

			bp::list group_ids;
			for (int g : groupset.group_ids()) {
				group_ids.append(g);
			}

			result["group_ids"] = group_ids;

			gil_guard_t gil_guard;
			result["hosts"] = detail::convert(groupset.hosts(), gil_guard);
			result["settings"] = detail::convert(groupset.settings(), gil_guard);

			return result;
		}

		bp::list
		get_couple_groupset_ids(int group) const {
			auto groupset_ids = impl.couples().get_couple_groupset_ids(group);

			bp::list result;

			for (auto it = groupset_ids.begin(), end = groupset_ids.end(); it != end; ++it) {
				result.append(*it);
			}

			return result;
		}

		// the method is always called from python's thread only
		bp::list
		get_couple_groups(int group) const {
			auto groups = impl.couples().get_couple_groups(group);

			bp::list result;

			for (auto it = groups.begin(), end = groups.end(); it != end; ++it) {
				result.append(*it);
			}

			return result;
		}

		// the method is always called from python's thread only
		bp::list
		get_groups(int group) const {
			auto groups = impl.couples().get_groups(group);

			bp::list result;

			for (auto it = groups.begin(), end = groups.end(); it != end; ++it) {
				result.append(*it);
			}

			return result;
		}

		bp::object
		hosts(int group) const {
			auto hosts = impl.couples().hosts(group);

			gil_guard_t gil_guard;
			return detail::convert(hosts, gil_guard);
		}

	private:
		mm::namespace_state_t impl;
	};

	class settings_t {
	public:
		settings_t(mm::namespace_state_t impl_)
			: impl(std::move(impl_))
		{}

		size_t
		groups_count() const {
			return impl.settings().groups_count();
		}

		const std::string &
		success_copies_num() const {
			return impl.settings().success_copies_num();
		}

		// the method is always called from python's thread only
		const bp::object &
		user_settings() const {
			return *(static_cast<const user_settings_t &>(impl.settings().user_settings()).dict);
		}

	private:
		mm::namespace_state_t impl;
	};

	namespace_state_t(mm::namespace_state_t impl_)
		: impl(std::move(impl_))
	{}

	couples_t
	couples() const {
		return {impl};
	}

	settings_t
	settings() const {
		return {impl};
	}

	const std::string &
	name() const {
		return impl.name();
	}

private:
	mm::namespace_state_t impl;
};

class mastermind_t {
public:
	// the constructor is always called from python's thread only
	mastermind_t(const std::string &remotes, int update_period, std::string cache_path
			, int warning_time, int expire_time, std::string worker_name
			, int enqueue_timeout, int reconnect_timeout, bp::object ns_filter_, bool auto_start)
	{
		gil_guard_t gil_guard;
		auto native_remotes = parse_remotes(remotes);
		auto logger = std::make_shared<python_logger_wrapper>(gil_guard);
		set_ns_filter(std::move(ns_filter_));
		gil_guard.release();

		py_allow_threads_scoped gil_release;
		impl = std::make_shared<mm::mastermind_t>(
				native_remotes,
				logger,
				update_period,
				std::move(cache_path),
				warning_time,
				expire_time,
				std::move(worker_name),
				enqueue_timeout,
				reconnect_timeout,
				// turn off built-in auto start, we'll start it manually a moment later
				false
		);

		impl->set_user_settings_factory([this] (const std::string &name
					, const kora::config_t &config) {
					return user_settings_factory(name, config);
				});

		if (auto_start) {
			start_impl(gil_release);
		}
	}

	// the destructor is always called from python's thread only
	~mastermind_t() {
		if (is_running()) {
			stop();
		}
	}

	void
	start() {
		py_allow_threads_scoped gil_release;
		start_impl(gil_release);
	}

	void
	stop() {
		py_allow_threads_scoped gil_release;
		impl->stop();
	}

	bool
	is_running() const {
		return impl->is_running();
	}

	bool
	is_valid() const {
		return impl->is_valid();
	}

	namespace_state_t
	get_namespace_state(const std::string &name) const {
		return {impl->get_namespace_state(name)};
	}

	namespace_state_t
	find_namespace_state(int group) const {
		return {impl->find_namespace_state(group)};
	}

	// the method is always called from python's thread only
	void
	set_ns_filter(bp::object ns_filter_) {
		ns_filter = ns_filter_;
	}

private:
	static
	std::vector<mm::mastermind_t::remote_t>
	parse_remotes(const std::string &remotes) {
		typedef boost::char_separator<char> separator_t;
		typedef boost::tokenizer<separator_t> tokenizer_t;

		std::vector<mm::mastermind_t::remote_t> result;

		separator_t sep1(",");
		tokenizer_t tok1(remotes, sep1);

		separator_t sep2(":");

		for (auto it = tok1.begin(), end = tok1.end(); it != end; ++it) {
			tokenizer_t tok2(*it, sep2);
			auto jt = tok2.begin();

			if (tok2.end() == jt) {
				throw std::runtime_error("remotes are malformed");
			}

			auto host = *jt++;
			uint16_t port = 10053;

			if (tok2.end() != jt) {
				port = boost::lexical_cast<uint16_t>(*jt++);
			}

			if (tok2.end() != jt) {
				throw std::runtime_error("remotes are malformed");
			}

			result.emplace_back(std::make_pair(std::move(host), port));
		}

		return result;
	}

	mm::namespace_state_t::user_settings_ptr_t
	user_settings_factory(const std::string &name, const kora::config_t &config) {
		gil_guard_t gil_guard;

		auto settings = detail::convert(config.underlying_object().as_object(), gil_guard);

		if (ns_filter) {
			try {
				if (!ns_filter(name, settings)) {
					return nullptr;
				}
			} catch (const std::exception &ex) {
				// ns_filter can throw python object that must be destroyed during gil is locked
				// that is during gil_guard is not out of scope.
				throw std::runtime_error(std::string{"ns_filter error: "} + ex.what());
			}
		}

		std::unique_ptr<namespace_state_t::user_settings_t> result{
			new namespace_state_t::user_settings_t{std::move(settings), gil_guard}
		};

		return mm::namespace_state_t::user_settings_ptr_t(std::move(result));
	}

	void
	start_impl(py_allow_threads_scoped &) {
		impl->start();
	}

	//XXX: boost.python does object copying at some point but can't cope with unique_ptr
	// and it's move semantic (not until boost 1.55 anyway).
	// Here:
	// - member object can not be used (mm::mastermind_t holds unique_ptr to implementation object),
	// - unique_ptr to mm::mastermind_t doesn't work either
	// So we forced to use shared_ptr.
	std::shared_ptr<mm::mastermind_t> impl;
	bp::object ns_filter;
};

namespace exception {

PyObject *
make_exception_class(const std::string &class_name, PyObject *base_type = PyExc_Exception) {
	std::string scope_name = bp::extract<std::string>(bp::scope().attr("__name__"));
	auto full_name = scope_name + '.' + class_name;
	char *raw_full_name = &full_name.front();

	auto *exception_type = PyErr_NewException(raw_full_name, base_type, 0);

	if (!exception_type) {
		bp::throw_error_already_set();
	}

	// bp::scope().attr(class_name) = bp::handle<>(bp::borrowed(exception_type));
	bp::scope().attr(class_name.c_str()) = bp::handle<>(exception_type);
	return exception_type;
}

namespace detail {

template <typename Ex>
std::string
exception_message(const Ex &ex) {
	return ex.what();
}

template <>
std::string
exception_message<mm::unknown_feedback>(const mm::unknown_feedback &ex) {
	std::ostringstream oss;
	oss << ex.what() << ": couple_id=" << ex.couple_id() << "; feedback=" << ex.feedback();
	return oss.str();
}

template <>
std::string
exception_message<mm::unknown_group_error>(const mm::unknown_group_error &ex) {
	std::ostringstream oss;
	oss << ex.what() << ": group=" << ex.group();
	return oss.str();
}

template <>
std::string
exception_message<mm::unknown_groupset_error>(const mm::unknown_groupset_error &ex) {
	std::ostringstream oss;
	oss << ex.what() << ": groupset=" << ex.groupset();
	return oss.str();
}

} // namespace detail

template <typename Ex>
void
register_exception_translator(const std::string &class_name, PyObject *base_type) {
	auto *exception_class = make_exception_class(class_name, base_type);

	auto translate = [exception_class] (const Ex &ex) {
		// PyErr_SetString(exception_class, ex.what());
		PyErr_SetString(exception_class, detail::exception_message(ex).c_str());
	};

	bp::register_exception_translator<Ex>(std::move(translate));
}

} // namespace exception

void
init_exception_translator() {
	auto *mastermind_cache_error
		= exception::make_exception_class("MastermindCacheError");

	//IMPORTANT: ensure that the following exception list is in sync with
	// the exception list in include/mastermind-cache/error.hpp

	exception::register_exception_translator<mm::couple_not_found_error>(
			"CoupleNotFoundError", mastermind_cache_error);

	exception::register_exception_translator<mm::not_enough_memory_error>(
			"NotEnoughMemoryError", mastermind_cache_error);

	exception::register_exception_translator<mm::unknown_namespace_error>(
			"UnknownNamespaceError", mastermind_cache_error);

	exception::register_exception_translator<mm::invalid_groups_count_error>(
			"InvalidGroupsCountError", mastermind_cache_error);

	exception::register_exception_translator<mm::cache_is_expired_error>(
			"CacheIsExpiredError", mastermind_cache_error);

	exception::register_exception_translator<mm::update_loop_already_started>(
			"UpdateLoopAlreadyStartedError", mastermind_cache_error);

	exception::register_exception_translator<mm::update_loop_already_stopped>(
			"UpdateLoopAlreadyStopped", mastermind_cache_error);

	exception::register_exception_translator<mm::unknown_feedback>(
			"UnknownFeedback", mastermind_cache_error);

	exception::register_exception_translator<mm::namespace_state_not_found_error>(
			"NamespaceNotFoundError", mastermind_cache_error);

	exception::register_exception_translator<mm::unknown_group_error>(
			"UnknownGroupError", mastermind_cache_error);

	exception::register_exception_translator<mm::unknown_groupset_error>(
			"UnknownGroupsetError", mastermind_cache_error);

	exception::register_exception_translator<mm::remotes_empty_error>(
			"RemotesEmptyError", mastermind_cache_error);

}

} // namespace binding
} // namespace mastermind

namespace mb = mastermind::binding;

BOOST_PYTHON_MODULE(mastermind_cache) {
	PyEval_InitThreads();

	mb::init_exception_translator();

	bp::class_<mb::namespace_state_t::couples_t>("Couples", bp::no_init)
		.def("get_couple_read_preference"
				, &mb::namespace_state_t::couples_t::get_couple_read_preference
				, (bp::arg("group")))
		.def("get_couple_groupset"
				, &mb::namespace_state_t::couples_t::get_couple_groupset
				, (bp::arg("group"), bp::arg("groupset")))
		.def("get_couple_groupset_ids"
				, &mb::namespace_state_t::couples_t::get_couple_groupset_ids
				, (bp::arg("group")))
		.def("get_couple_groups"
				, &mb::namespace_state_t::couples_t::get_couple_groups
				, (bp::arg("group")))
		.def("get_groups"
				, &mb::namespace_state_t::couples_t::get_groups
				, (bp::arg("group")))
		.def("hosts"
				, &mb::namespace_state_t::couples_t::hosts
				, (bp::arg("group")))
		;

	bp::class_<mb::namespace_state_t::settings_t>("Settings", bp::no_init)
		.def("groups_count", &mb::namespace_state_t::settings_t::groups_count)
		.def("success_copies_num", bp::make_function(
					&mb::namespace_state_t::settings_t::success_copies_num
					, bp::return_value_policy<bp::copy_const_reference>()))
		.def("user_settings", bp::make_function(
					&mb::namespace_state_t::settings_t::user_settings
					, bp::return_value_policy<bp::copy_const_reference>()))
		;

	bp::class_<mb::namespace_state_t>("NamespaceState", bp::no_init)
		.def("couples", &mb::namespace_state_t::couples)
		.def("settings", &mb::namespace_state_t::settings)
		.def("name", bp::make_function(&mb::namespace_state_t::name
					, bp::return_value_policy<bp::copy_const_reference>()))
		;

	bp::class_<mb::mastermind_t>("MastermindCache"
			, bp::init<const std::string &, int, std::string, int, int, std::string, int, int, bp::object, bool>((
				bp::arg("remotes")
				, bp::arg("update_period") = 60
				, bp::arg("cache_path") = std::string{}
				, bp::arg("warning_time") = std::numeric_limits<int>::max()
				, bp::arg("expire_time") = std::numeric_limits<int>::max()
				, bp::arg("worker_name") = std::string{"mastermind2.26"}
				, bp::arg("enqueue_timeout") = 4000
				, bp::arg("reconnect_timeout") = 4000
				, bp::arg("ns_filter") = bp::object()
				, bp::arg("auto_start") = true
			))
		)
		.def("start", &mb::mastermind_t::start)
		.def("stop", &mb::mastermind_t::stop)
		.def("is_running", &mb::mastermind_t::is_running)
		.def("is_valid", &mb::mastermind_t::is_valid)
		.def("get_namespace_state", &mb::mastermind_t::get_namespace_state
				, (bp::arg("name")))
		.def("find_namespace_state", &mb::mastermind_t::find_namespace_state
				, (bp::arg("group")))
		.def("set_ns_filter", &mb::mastermind_t::set_ns_filter, (bp::arg("ns_filter")))
		;
}
