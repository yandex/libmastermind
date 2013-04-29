#include "elliptics/proxy.hpp"
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include <sstream>
#include <memory>

using namespace elliptics;
using namespace boost::python;

std::string remote_str(const elliptics_proxy_t::remote &ob) {
	std::ostringstream oss;
	oss << ob.host << ':' << ob.port << ':' << ob.family;
	return oss.str();
}

std::string remote_repr(const elliptics_proxy_t::remote &ob) {
	return std::string("remote: ") + remote_str(ob);
}

class python_config : public elliptics_proxy_t::config {
public:
	elliptics_proxy_t::config &convert() {
		//elliptics_proxy_t::config res(*this);
		remotes.clear();
		size_t l = len(remotes_list);
		remotes.reserve(l);
		for (size_t index = 0; index != l; ++index) {
			remotes.push_back(extract<elliptics_proxy_t::remote>(remotes_list[index]));
		}
		return *this;
	}

	list remotes_list;
};

std::string config_str(const python_config &ob) {
	std::ostringstream oss;

	oss << "remotes = [";
	size_t l = len(ob.remotes_list);
	for (size_t index = 0; index != l; ++index) {
		if (index != 0) oss << ' ';
		oss << remote_str(extract<elliptics_proxy_t::remote>(ob.remotes_list[index]));
	}
	oss << "] ";
	
	oss << "groups = [";
	for (auto it = ob.groups.begin(); it != ob.groups.end(); ++it) {
		if (it != ob.groups.begin()) oss << ' ';
		oss << *it;
	}
	oss << "] ";

	return oss.str();
}

std::string config_repr(const python_config &ob) {
	return std::string("config: ") + config_str(ob);
}

template<typename T>
std::string vector_str(const std::vector<T> &v) {
	std::ostringstream oss;
	oss << "[";
	for (auto it = v.begin(); it != v.end(); ++it) {
		if (it != v.begin()) oss << ", ";
		oss << *it;
	}
	oss << "]";
	return oss.str();
}

template<typename T>
std::string vector_repr(const std::vector<T> &v) {
	return std::string("list: ").append(vector_str(v));
}

std::string key_str(const elliptics::key_t &key) {
	return key.to_string();
}

std::string key_repr(const elliptics::key_t &key) {
//	return std::string("key_t: ").append(key_str(key));
	return key_str(key);
}

std::string lookup_result_str(const lookup_result_t &lr) {
	std::ostringstream oss;
	oss << "groups: " << lr.group << "\tpath: " << lr.hostname << ":" << lr.port << lr.path;
	return oss.str();
}

std::string lookup_result_repr(const lookup_result_t &lr) {
	return lookup_result_str(lr);
}

class python_data_container_t : public data_container_t {
public:
	python_data_container_t() {}

	python_data_container_t(const std::string &message)
		: data_container_t(message)
	{}

	python_data_container_t(const data_container_t &dc)
		: data_container_t(dc)
	{
	}

	std::string get_data() {
		return data.to_string();
	}

	void set_data(const std::string &message) {
		data = std::move(ioremap::elliptics::data_buffer(message.data(), message.size()));
	}

	timespec get_timestamp() {
		return *get<DNET_FCGI_EMBED_TIMESTAMP>();
	}

	void set_timestamp(const timespec &ts) {
		set<DNET_FCGI_EMBED_TIMESTAMP>(ts);
	}
};

template<typename T>
class python_async_result_t {
public:
	typedef ioremap::elliptics::async_result<T> async_result_t;

	python_async_result_t()
	{
	}

	python_async_result_t(async_result_t &&async_result)
		: m_async_result(new async_result_t(std::move(async_result)))
	{
	}

	list get() {
		list res;

		if (!m_async_result)
			throw std::runtime_error("No async result is related");

		auto v = m_async_result->get();
		for (auto it = v.begin(); it != v.end(); ++it)
			res.append(*it);

		return res;
	}

	T get_one() {
		if (!m_async_result)
			throw std::runtime_error("No async result is related");
		return m_async_result->get_one();
	}
private:
	std::shared_ptr<async_result_t> m_async_result;
};

typedef python_async_result_t<ioremap::elliptics::write_result_entry> python_async_write_result_t;
typedef python_async_result_t<ioremap::elliptics::callback_result_entry> python_async_remove_result_t;

class python_async_read_result_t {
public:
	python_async_read_result_t()
	{
	}

	python_async_read_result_t(async_read_result_t &&async_read_result)
		: m_async_read_result(new async_read_result_t(std::move(async_read_result)))
	{
	}

	python_data_container_t get() {
		if (!m_async_read_result)
			throw std::runtime_error("No async result is related");
		return m_async_read_result->get();
	}
private:
	std::shared_ptr<async_read_result_t> m_async_read_result;
};

class python_elliptics_proxy_t : public elliptics_proxy_t {
public:
	typedef elliptics_proxy_t base;

	python_elliptics_proxy_t(python_config &conf)
		: elliptics_proxy_t(conf.convert())
	{
	}

	lookup_result_t lookup(const elliptics::key_t &key,
			const std::vector<int> &groups = std::vector<int>()) {
		return base::lookup(key, _groups = groups);
	}

	python_data_container_t read(const elliptics::key_t &key,
			const uint64_t &offset = 0, const uint64_t &size = 0,
			const uint64_t &cflags = 0, const uint64_t &ioflags = 0,
			const std::vector<int> &groups = std::vector<int>(),
			bool latest = false, bool embeded = false) {
		return base::read(key,
							_offset = offset, _size = size,
							_cflags = cflags, _ioflags = ioflags,
							_groups = groups,
							_latest = latest, _embeded = embeded);
	}

	list write(const elliptics::key_t &key, const python_data_container_t &dc,
			const uint64_t &offset = 0, const uint64_t &size = 0,
			const uint64_t &cflags = 0, const uint64_t &ioflags = 0,
			const std::vector<int> &groups = std::vector<int>(),
			int success_copies_num = 0) {
		auto lrs = base::write(key, dc,
								_offset = offset, _size = size,
								_cflags = cflags, _ioflags = ioflags,
								_groups = groups,
								_success_copies_num = success_copies_num);
		list res;
		for (auto it = lrs.begin(); it != lrs.end(); ++it)
			res.append(*it);
		return res;
	}

	void remove(const elliptics::key_t &key,
			const std::vector<int> &groups = std::vector<int>()) {
		base::remove(key, _groups = groups);
	}

	std::vector<std::string> range_get(
			const elliptics::key_t &from, const elliptics::key_t &to,
			const uint64_t &limit_start = 0, const uint64_t &limit_num = 0,
			const uint64_t &cflags = 0, const uint64_t &ioflags = 0,
			const std::vector<int> &groups = std::vector<int>(),
			const elliptics::key_t &key = elliptics::key_t()) {
		return base::range_get(from, to,
								_limit_start = limit_start, _limit_num = limit_num,
								_cflags = cflags, _ioflags = ioflags,
								_groups = groups,
								_key = key);
	}

	dict bulk_read(const list &keys,
					const uint64_t &cflags = 0,
					const std::vector<int> &groups = std::vector<int>()) {
		std::vector<elliptics::key_t> ks;
		size_t l = len(keys);
		ks.reserve(l);
		for (size_t index = 0; index != l; ++index)
			ks.push_back(extract<elliptics::key_t>(keys[index]));

		auto dcs = base::bulk_read(ks, _cflags = cflags, _groups = groups);

		dict res;

		for (auto it = dcs.begin(); it != dcs.end(); ++it) {
			res[it->first] = python_data_container_t(it->second);
		}

		return res;
	}

	list lookup_addr(const elliptics::key_t &key,
						const std::vector<int> &groups = std::vector<int>()) {
		auto rs = base::lookup_addr(key, _groups = groups);

		list res;

		for (auto it = rs.begin(); it != rs.end(); ++it) {
			res.append(*it);
		}

		return res;
	}

	dict bulk_write(const list &keys, const list &data,
					const uint64_t &cflags = 0,
					const std::vector<int> &groups = std::vector<int>(),
					int success_copies_num = 0
					) {
		size_t l = len(keys);
		std::vector<elliptics::key_t> ks;
		std::vector<data_container_t> dcs;
		ks.reserve(l);
		dcs.reserve(l);

		for (size_t index = 0; index != l; ++index) {
			ks.push_back(extract<elliptics::key_t>(keys[index]));
			dcs.push_back(extract<python_data_container_t>(data[index]));
		}

		auto lrs = base::bulk_write(ks, dcs,
										_cflags = cflags, _groups = groups,
										_success_copies_num = success_copies_num);

		dict res;

		for (auto it = lrs.begin(); it != lrs.end(); ++it) {
			list lst;

			for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
				lst.append(*it2);
			}

			res[it->first] = lst;
		}

		return res;
	}

	std::string exec_script(const elliptics::key_t &key,
								const std::string &script, const std::string &data,
								const std::vector<int> &groups = std::vector<int>()) {
		return base::exec_script(key, script, data, _groups = groups);
	}

	python_async_read_result_t read_async(const elliptics::key_t &key,
			const uint64_t &offset = 0, const uint64_t &size = 0,
			const uint64_t &cflags = 0, const uint64_t &ioflags = 0,
			const std::vector<int> &groups = std::vector<int>(),
			bool latest = false, bool embeded = false) {
		return std::move(base::read_async(key,
											_offset = offset, _size = size,
											_cflags = cflags, _ioflags = ioflags,
											_groups = groups,
											_latest = latest, _embeded = embeded));
	}
};

BOOST_PYTHON_MODULE(elliptics_proxy)
{
	class_<std::vector<int> >("VecInt")
		.def(vector_indexing_suite<std::vector<int> >())
		.def("__str__", &vector_str<int>)
		.def("__repr__", &vector_repr<int>)
	;

	class_<std::vector<std::string> >("VecString")
		.def(vector_indexing_suite<std::vector<std::string> >())
		.def("__str__", &vector_str<std::string>)
		.def("__repr__", &vector_repr<std::string>)
	;

	class_<timespec>("timespec")
		.def_readwrite("tv_sec", &timespec::tv_sec)
		.def_readwrite("tv_nsec", &timespec::tv_nsec)
	;

	class_<elliptics_proxy_t::remote>("remote", init<const std::string &, int, optional<int> >())
		.def("__str__", remote_str)
		.def("__repr__", remote_repr)
		.def_readwrite("host", &elliptics_proxy_t::remote::host)
		.def_readwrite("port", &elliptics_proxy_t::remote::port)
		.def_readwrite("family", &elliptics_proxy_t::remote::family)
	;

	class_<python_config>("config")
		.def("__str__", config_str)
		.def("__repr__", config_repr)
		.def_readwrite("log_path",&python_config::log_path)
		.def_readwrite("log_mask",&elliptics_proxy_t::config::log_mask)
		.add_property("remotes",&python_config::remotes_list)
		.def_readwrite("flags",&elliptics_proxy_t::config::flags)
		.def_readwrite("ns",&elliptics_proxy_t::config::ns)
		.def_readwrite("wait_timeout",&elliptics_proxy_t::config::wait_timeout)
		.def_readwrite("check_timeout",&elliptics_proxy_t::config::check_timeout)
		.def_readwrite("groups",&python_config::groups)	
		.def_readwrite("base_port",&elliptics_proxy_t::config::base_port)
		.def_readwrite("directory_bit_num",&elliptics_proxy_t::config::directory_bit_num)
		.def_readwrite("success_copies_num",&elliptics_proxy_t::config::success_copies_num)
		.def_readwrite("die_limit",&elliptics_proxy_t::config::die_limit)
		.def_readwrite("replication_count",&elliptics_proxy_t::config::replication_count)
		.def_readwrite("chunk_size",&elliptics_proxy_t::config::chunk_size)
		.def_readwrite("eblob_style_path",&elliptics_proxy_t::config::eblob_style_path)
#ifdef HAVE_METABASE
		.def_readwrite("cocaine_config",&elliptics_proxy_t::config::cocaine_config)
		.def_readwrite("group_weights_refresh_period",
						&elliptics_proxy_t::config::group_weights_refresh_period)
#endif /* HAVE_METABASE */
	;

	class_<dnet_id>("dnet_id")
		.add_property("id", make_getter(&dnet_id::id))
		.def_readwrite("group_id", &dnet_id::group_id)
		.def_readwrite("type", &dnet_id::type)
	;

	class_<elliptics::key_t>("key_t", init<const std::string &, optional<int> >())
		.def(init<const dnet_id &>())
		.def("__str__", key_str)
		.def("__repr__", key_repr)
		.def_readonly("by_id", &elliptics::key_t::by_id)
		.add_property("remote",
			make_function(
				static_cast<const std::string &(elliptics::key_t::*)() const>(
					&elliptics::key_t::remote),
				return_value_policy<copy_const_reference>()
				)
			)
		.def_readonly("type", &elliptics::key_t::type)
		.def_readonly("id",
			make_function(
				static_cast<const dnet_id &(elliptics::key_t::*)() const>(
					&elliptics::key_t::id),
				return_value_policy<copy_const_reference>()
				)
			)
	;
	class_<lookup_result_t>("lookup_result_t")
		.def("__str__", lookup_result_str)
		.def("__repr__", lookup_result_repr)
		.def_readwrite("hostname", &lookup_result_t::hostname)
		.def_readwrite("port", &lookup_result_t::port)
		.def_readwrite("path", &lookup_result_t::path)
		.def_readwrite("group", &lookup_result_t::group)
		.def_readwrite("status", &lookup_result_t::status)
		.def_readwrite("addr", &lookup_result_t::addr)
		.def_readwrite("short_path", &lookup_result_t::short_path)
	;

	class_<python_data_container_t>("data_container_t")
		.def(init<const std::string &>())
		.add_property("data", &python_data_container_t::get_data,
								&python_data_container_t::set_data)
		.add_property("timestamp", &python_data_container_t::get_timestamp,
								&python_data_container_t::set_timestamp)
	;

	class_<python_async_read_result_t>("async_read_result_t")
		.def("get", &python_async_read_result_t::get)
	;

	class_<python_async_write_result_t>("async_write_result_t")
		.def("get", &python_async_write_result_t::get)
		.def("get_one", &python_async_write_result_t::get_one)
	;

	class_<python_async_remove_result_t>("async_remove_result_t")
		.def("get", &python_async_remove_result_t::get)
		.def("get_one", &python_async_remove_result_t::get_one)
	;

	class_<python_elliptics_proxy_t, boost::noncopyable>("elliptics_proxy_t", init<python_config &>())
		.def("lookup", &python_elliptics_proxy_t::lookup,
			(arg("key"), arg("groups") = std::vector<int>()))
		.def("read", &python_elliptics_proxy_t::read,
			(arg("key"),
			arg("offset") = 0, arg("size") = 0,
			arg("cflags") = 0, arg("ioflags") = 0,
			arg("groups") = std::vector<int>(),
			arg("latest") = false, arg("embeded") = false))
		.def("write", &python_elliptics_proxy_t::write,
			(arg("key"), arg("dc"),
			arg("offset") = 0, arg("size") = 0,
			arg("cflags") = 0, arg("ioflags") = 0,
			arg("groups") = std::vector<int>(),
			arg("success_copies_num") = 0))
		.def("remove", &python_elliptics_proxy_t::remove,
			(arg("key"),
			arg("groups") = std::vector<int>()))
		.def("range_get", &python_elliptics_proxy_t::range_get,
			(arg("from"), arg("to"),
			arg("limit_start") = 0, arg("limit_num") = 0,
			arg("cflags") = 0, arg("ioflags") = 0,
			arg("groups") = std::vector<int>(),
			arg("key") = elliptics::key_t()))
		.def("bulk_read", &python_elliptics_proxy_t::bulk_read,
			(arg("keys"),
			arg("cflags") = 0,
			arg("groups") = std::vector<int>()))
		.def("lookup_addr", &python_elliptics_proxy_t::lookup_addr,
			(arg("key"), arg("groups") = std::vector<int>()))
		.def("bulk_write", &python_elliptics_proxy_t::bulk_write,
			(arg("keys"), arg("dcs"),
			arg("cflags") = 0,
			arg("groups") = std::vector<int>(),
			arg("success_copies_num") = 0))
		.def("exec_script", &python_elliptics_proxy_t::exec_script,
			(arg("key"), arg("script"), arg("data"),
			arg("groups") = std::vector<int>()))
		.def("read_async", &python_elliptics_proxy_t::read_async,
			(arg("key"),
			arg("offset") = 0, arg("size") = 0,
			arg("cflags") = 0, arg("ioflags") = 0,
			arg("groups") = std::vector<int>(),
			arg("latest") = false, arg("embeded") = false))
	;
}
