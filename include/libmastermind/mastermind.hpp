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

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <boost/optional.hpp>

#include <blackhole/logger.hpp>

#include <kora/dynamic.hpp>
#include <kora/config.hpp>

#include <libmastermind/common.hpp>
#include <libmastermind/error.hpp>
#include <libmastermind/couple_sequence.hpp>

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
	bool is_active() const;

	bool can_choose_couple_to_upload() const;
	int64_t multipart_content_length_threshold() const;

	int redirect_expire_time() const;
	int64_t redirect_content_length_threshold() const;

private:
	std::unique_ptr<data> m_data;
};

class namespace_state_t {
public:
	class user_settings_t {
	public:
		virtual ~user_settings_t();
	};

	typedef std::unique_ptr<user_settings_t> user_settings_ptr_t;

	typedef
	std::function<user_settings_ptr_t (const std::string &name, const kora::config_t &config)>
	user_settings_factory_t;

	class settings_t {
	public:
		size_t groups_count() const;
		const std::string &success_copies_num() const;

		const user_settings_t &user_settings() const;

	private:
		friend class namespace_state_t;

		settings_t(const namespace_state_t &namespace_state_);

		const namespace_state_t &namespace_state;
	};

	class couples_t;

	class groupset_t {
	public:
		uint64_t free_effective_space() const;
		uint64_t free_reserved_space() const;

		std::string type() const;
		std::string status() const;
		std::string id() const;

		const std::vector<int> &group_ids() const;
		const kora::dynamic_t &hosts() const;
		const kora::dynamic_t &settings() const;

	private:
		friend class couples_t;

		struct data_t;
		groupset_t(const data_t &data_);

		const data_t &data;
	};

	class couples_t {
	public:
		std::vector<std::string> get_couple_read_preference(group_t group) const;
		groupset_t get_couple_groupset(group_t group, const std::string &groupset_id) const;

		std::vector<std::string> get_couple_groupset_ids(group_t group) const;

		groups_t get_couple_groups(group_t group) const;
		groups_t get_groups(group_t group) const;

		uint64_t free_effective_space(group_t group) const;
		uint64_t free_reserved_space(group_t group) const;
		kora::dynamic_t hosts(group_t group) const;
	private:
		friend class namespace_state_t;

		couples_t(const namespace_state_t &namespace_state_);

		const namespace_state_t &namespace_state;
	};

	class weights_t {
	public:
		enum class feedback_tag {
			  available
			, partly_unavailable
			, temporary_unavailable
			, permanently_unavailable
		};

		groups_t groups(uint64_t size = 0) const;
		couple_sequence_t couple_sequence(uint64_t size = 0) const;
		void set_feedback(group_t couple_id, feedback_tag feedback);

	private:
		friend class namespace_state_t;

		weights_t(const namespace_state_t &namespace_state_);

		const namespace_state_t &namespace_state;
	};

	class statistics_t {
	public:
		bool
		ns_is_full();
	private:
		friend class namespace_state_t;

		statistics_t(const namespace_state_t &namespace_state_);

		const namespace_state_t &namespace_state;
	};

	settings_t settings() const;

	couples_t couples() const;

	weights_t weights() const;

	statistics_t statistics() const;

	const std::string &name() const;

	operator bool() const;

protected:
	class data_t;

	std::shared_ptr<data_t> data;

private:
};

class mastermind_t {
public:
	typedef std::pair<std::string, uint16_t> remote_t;
	typedef std::vector<remote_t> remotes_t;

	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period = 60);
	mastermind_t(const std::string &host, uint16_t port,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period = 60);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period, std::string cache_path, int expired_time,
			std::string worker_name);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time, int expire_time,
			std::string worker_name,
			int enqueue_timeout,
			int reconnect_timeout);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time, int expire_time,
			std::string worker_name,
			int enqueue_timeout,
			int reconnect_timeout,
			namespace_state_t::user_settings_factory_t user_settings_factory
			);
	mastermind_t(const remotes_t &remotes,
			const std::shared_ptr<blackhole::logger_t> &logger,
			int group_info_update_period, std::string cache_path,
			int warning_time, int expire_time,
			std::string worker_name,
			int enqueue_timeout,
			int reconnect_timeout,
			bool auto_start
			);

	~mastermind_t();

	void
	start();

	void
	stop();

	bool
	is_running() const;

	bool
	is_valid() const;

	std::vector<int> get_metabalancer_groups(uint64_t count = 0, const std::string &name_space = std::string("default"), uint64_t size = 0);
	// group_info_response_t get_metabalancer_group_info(int group);
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

	namespace_state_t
	get_namespace_state(const std::string &name) const;

	namespace_state_t
	find_namespace_state(group_t group) const;

	groups_t
	get_cached_groups(const std::string &elliptics_id, group_t couple_id) const;

	std::string json_group_weights();
	std::string json_symmetric_groups();
	std::string json_bad_groups();
	std::string json_cache_groups();
	std::string json_metabalancer_info();
	std::string json_namespaces_settings();
	std::string json_namespace_statistics(const std::string &ns);

	void
	set_user_settings_factory(namespace_state_t::user_settings_factory_t user_settings_factory);

	void cache_force_update();
	void set_update_cache_callback(const std::function<void (void)> &callback);
	void set_update_cache_ext1_callback(const std::function<void (bool)> &callback);

private:
	struct data;
	std::unique_ptr<data> m_data;
};

} // namespace mastermind

#endif /* INCLUDE__LIBMASTERMIND__MASTERMIND_HPP */

