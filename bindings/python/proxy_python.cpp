#include "elliptics/proxy.hpp"
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include <sstream>

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

BOOST_PYTHON_MODULE(elliptics_proxy)
{
	boost::python::class_<std::vector<int> >("VecInt")
	       .def(boost::python::vector_indexing_suite<std::vector<int> >())
		   .def("__str__", &vector_str<int>)
		   .def("__repr__", &vector_repr<int>)
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
}
