#ifndef SRC__UTILS_HPP
#define SRC__UTILS_HPP

#include "libmastermind/mastermind.hpp"

#include <cocaine/framework/logging.hpp>

#include <string>
#include <tuple>
#include <functional>
#include <chrono>

namespace mastermind {

class spent_time_printer_t {
public:
	spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger);

	~spent_time_printer_t();

private:
	std::string m_handler_name;
	std::shared_ptr<cocaine::framework::logger_t> &m_logger;
	std::chrono::system_clock::time_point m_beg_time;
};

class metabalancer_groups_info_t {
public:
	typedef std::vector<int> couple_t;
	typedef std::tuple<couple_t, uint64_t, uint64_t> couple_with_info_t;
	typedef std::vector<couple_with_info_t> couples_with_info_t;
	typedef std::map<uint64_t, couples_with_info_t> couples_t;
	typedef std::map<std::string, couples_t> namespaces_t;

	metabalancer_groups_info_t();
	metabalancer_groups_info_t(namespaces_t &&namespaces);

	std::vector<int> get_couple(uint64_t count, const std::string &name, uint64_t size);
	bool empty();
	std::string to_string();
	const namespaces_t &data() const;

private:
	static bool couples_with_info_comp(const couple_with_info_t &c1, const couple_with_info_t &c2);

	typedef std::reference_wrapper<const couple_t> const_couple_ref_t;
	typedef std::map<uint64_t, const_couple_ref_t> weighted_couples_t;
	typedef std::map<uint64_t, weighted_couples_t> couples_by_avalible_memory_t;
	typedef std::map<uint64_t, couples_by_avalible_memory_t> couples_by_groups_count_t;
	typedef std::map<std::string, couples_by_groups_count_t> couples_by_namespaces_t;

	namespaces_t m_namespaces;
	couples_by_namespaces_t m_couples;
};

enum GROUP_INFO_STATUS {
  GROUP_INFO_STATUS_OK,
  GROUP_INFO_STATUS_BAD,
  GROUP_INFO_STATUS_COUPLED
};

} // namespace mastermind

namespace msgpack {
mastermind::group_info_response_t &operator >> (object o, mastermind::group_info_response_t &v);
std::vector<mastermind::namespace_settings_t> &operator >> (object o, std::vector<mastermind::namespace_settings_t> &v);
packer<sbuffer> &operator << (packer<sbuffer> &o, const std::vector<mastermind::namespace_settings_t> &v);
} // namespace msgpack

#endif /* SRC__UTILS_HPP */
