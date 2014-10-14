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

#ifndef INCLUDE__LIBMASTERMIND__MASTERMIND_HPP
#define INCLUDE__LIBMASTERMIND__MASTERMIND_HPP

#include <libmastermind/error.hpp>

#include <cocaine/framework/logging.hpp>

#include <boost/optional.hpp>

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
	struct data;

	namespace_settings_t();
	namespace_settings_t(const namespace_settings_t &ns);
	namespace_settings_t(namespace_settings_t &&ns);
	namespace_settings_t(data &&d);

	~namespace_settings_t();

	namespace_settings_t &operator =(namespace_settings_t &&ns);

	const std::string &name() const;
	int groups_count() const;
	const std::string &success_copies_num() const;
	const std::string &auth_key() const;
	const std::vector<int> &static_couple() const;
	const std::string &sign_token() const;
	const std::string &sign_path_prefix() const;
	const std::string &sign_port() const;
	const std::string &auth_key_for_write() const;
	const std::string &auth_key_for_read() const;
	size_t content_length_threshold() const;
	bool is_active() const;

	bool can_choose_couple_to_upload() const;
	int64_t multipart_content_length_threshold() const;

private:
	std::unique_ptr<data> m_data;
};

class mastermind_t {
public:
	typedef std::pair<std::string, uint16_t> remote_t;
	typedef std::vector<remote_t> remotes_t;

	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<cocaine::framework::logger_t> &logger,
			int group_info_update_period = 60);
	mastermind_t(const std::string &host, uint16_t port,
			const std::shared_ptr<cocaine::framework::logger_t> &logger,
			int group_info_update_period = 60);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<cocaine::framework::logger_t> &logger,
			int group_info_update_period, std::string cache_path, int expired_time,
			std::string worker_name);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<cocaine::framework::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time, int expire_time,
			std::string worker_name,
			int enqueue_timeout,
			int reconnect_timeout);
	~mastermind_t();

	std::vector<int> get_metabalancer_groups(uint64_t count = 0, const std::string &name_space = std::string("default"), uint64_t size = 0);
	group_info_response_t get_metabalancer_group_info(int group);
	std::map<int, std::vector<int>> get_symmetric_groups();
	std::vector<int> get_symmetric_groups(int group);
	std::vector<int> get_couple_by_group(int group);
	std::vector<int> get_couple(int couple_id, const std::string &ns);
	std::vector<std::vector<int> > get_bad_groups();
	std::vector<int> get_all_groups();
	std::vector<int> get_cache_groups(const std::string &key);
	std::vector<namespace_settings_t> get_namespaces_settings();
	std::vector<std::string> get_elliptics_remotes();
	std::vector<std::tuple<std::vector<int>, uint64_t, uint64_t>> get_couple_list(const std::string &ns);

	uint64_t free_effective_space_in_couple_by_group(size_t group);

	std::string json_group_weights();
	std::string json_symmetric_groups();
	std::string json_bad_groups();
	std::string json_cache_groups();
	std::string json_metabalancer_info();
	std::string json_namespaces_settings();
	std::string json_namespace_statistics(const std::string &ns);

	void cache_force_update();
	void set_update_cache_callback(const std::function<void (void)> &callback);
	void set_update_cache_ext1_callback(const std::function<void (bool)> &callback);

private:
	struct data;
	std::unique_ptr<data> m_data;
};

} // namespace mastermind

#endif /* INCLUDE__LIBMASTERMIND__MASTERMIND_HPP */

