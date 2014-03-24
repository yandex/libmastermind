// elliptics-fastcgi - FastCGI-module component for Elliptics file storage
// Copyright (C) 2011 Leonid A. Movsesjan <lmovsesjan@yandex-team.ru>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#ifndef INCLUDE__LIBMASTERMIND__MASTERMIND_HPP
#define INCLUDE__LIBMASTERMIND__MASTERMIND_HPP

#include <libmastermind/error.hpp>

#include <cocaine/framework/logging.hpp>

#include <map>
#include <string>
#include <vector>
#include <memory>

namespace mastermind {

struct group_info_response_t {
	std::vector<std::string> nodes;
	std::vector<int> couples;
	int status;
	std::string name_space;
};

struct namespace_settings_t {
	std::string name;
	int groups_count;
	std::string success_copies_num;
	std::string auth_key;
	std::vector<int> static_couple;
};

class mastermind_t {
public:
	typedef std::pair<std::string, uint16_t> remote_t;
	typedef std::vector<remote_t> remotes_t;

	mastermind_t(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60);
	mastermind_t(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60);
	~mastermind_t();

	std::vector<int> get_metabalancer_groups(uint64_t count = 0, const std::string &name_space = std::string("default"), uint64_t size = 0);
	group_info_response_t get_metabalancer_group_info(int group);
	std::map<int, std::vector<int>> get_symmetric_groups();
	std::vector<int> get_symmetric_groups(int group);
	std::vector<std::vector<int> > get_bad_groups();
	std::vector<int> get_all_groups();
	std::vector<int> get_cache_groups(const std::string &key);
	std::vector<namespace_settings_t> get_namespaces_settings();

	std::string json_group_weights();
	std::string json_symmetric_groups();
	std::string json_bad_groups();
	std::string json_cache_groups();
	std::string json_namespaces_settings();

	void cache_force_update();
	void set_update_cache_callback(const std::function<void (void)> &callback);

private:
	struct data;
	std::unique_ptr<data> m_data;
};

} // namespace mastermind

#endif /* INCLUDE__LIBMASTERMIND__MASTERMIND_HPP */

