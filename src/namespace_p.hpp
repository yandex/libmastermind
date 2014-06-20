#ifndef SRC__NAMESPACE_P_HPP
#define SRC__NAMESPACE_P_HPP

#include "libmastermind/mastermind.hpp"

namespace mastermind {

struct namespace_settings_t::data {

	data();
	data(const data &d);
	data(data &&d);

	std::string name;
	int groups_count;
	std::string success_copies_num;
	std::string auth_key;
	std::vector<int> static_couple;

	std::string auth_key_for_write;
	std::string auth_key_for_read;

	std::string sign_token;
	std::string sign_path_prefix;
	std::string sign_port;
};

} // mastermind

#endif /* SRC__NAMESPACE_P_HPP */

