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

#include "libmastermind/mastermind.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/tokenizer.hpp>

#include <memory>
#include <iostream>
#include <cstdlib>
#include <limits>

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

class logger_t : public cocaine::framework::logger_t {
public:
	typedef cocaine::logging::priorities verbosity_t;

	logger_t(const gil_guard_t &)
		: logging(new bp::object{bp::import("logging")})
		, logger(new bp::object{logging->attr("getLogger")("mastermind_cache")})
	{
	}

	void emit(verbosity_t priority, const std::string& message) {
		gil_guard_t gil_guard;
		logger->attr(get_name(priority))("%s", message.c_str());
	}

	verbosity_t verbosity() const {
		return verbosity_t::debug;
	}

	~logger_t() {
		gil_guard_t gil_guard;
		logger.reset();
		logging.reset();
	}

private:
	static
	const char *
	get_name(verbosity_t priority) {
		switch (priority) {
		case verbosity_t::debug:
			return "debug";
		case verbosity_t::info:
			return "info";
		case verbosity_t::warning:
			return "warning";
		case verbosity_t::error:
			return "error";
		default:
			return "unknown";
		}
	}

	std::unique_ptr<bp::object> logging;
	std::unique_ptr<bp::object> logger;
};

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
		auto logger = std::make_shared<logger_t>(gil_guard);
		set_ns_filter(std::move(ns_filter_));
		gil_guard.release();

		py_allow_threads_scoped gil_release;
		impl = std::make_shared<mm::mastermind_t>(
				native_remotes, logger, update_period, std::move(cache_path)
				, warning_time, expire_time, std::move(worker_name)
				, enqueue_timeout, reconnect_timeout, false);

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

	std::shared_ptr<mm::mastermind_t> impl;
	bp::object ns_filter;
};

} // namespace binding
} // namespace mastermind

namespace mb = mastermind::binding;

BOOST_PYTHON_MODULE(mastermind_cache) {
	PyEval_InitThreads();

	bp::class_<mb::namespace_state_t::couples_t>("Couples", bp::no_init)
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
			, bp::init<const std::string &, int, std::string, int, int, std::string, int, int
				, bp::object, bool>(
				(bp::arg("remotes"), bp::arg("update_period") = 60
				 , bp::arg("cache_path") = std::string{}
				 , bp::arg("warning_time") = std::numeric_limits<int>::max()
				 , bp::arg("expire_time") = std::numeric_limits<int>::max()
				 , bp::arg("worker_name") = std::string{"mastermind2.26"}
				 , bp::arg("enqueue_timeout") = 4000
				 , bp::arg("reconnect_timeout") = 4000
				 , bp::arg("ns_filter") = bp::object()
				 , bp::arg("auto_start") = true
				 )))
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

