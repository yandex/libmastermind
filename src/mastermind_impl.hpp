#ifndef SRC__MASTERMIND_IMPL_HPP
#define SRC__MASTERMIND_IMPL_HPP

#include "libmastermind/mastermind.hpp"
#include "utils.hpp"

#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <fstream>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/common.hpp>
#include <cocaine/traits/tuple.hpp>

namespace mastermind {

struct mastermind_t::data {
	data(const remotes_t &remotes, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60);
	data(const std::string &host, uint16_t port, const std::shared_ptr<cocaine::framework::logger_t> &logger, int group_info_update_period = 60);
	~data();

	void reconnect();

	template <typename F, typename R, typename... Args>
	void retry(F func, R &res, Args &&... args);

	bool collect_group_weights();
	bool collect_bad_groups();
	bool collect_symmetric_groups();
	bool collect_cache_groups();
	void collect_info_loop();

	void serialize();
	void deserialize();

	std::shared_ptr<cocaine::framework::logger_t> m_logger;

	remotes_t                                          m_remotes;
	remote_t                                           m_current_remote;
	size_t                                             m_next_remote;

	int                                                m_metabase_timeout;
	uint64_t                                           m_metabase_current_stamp;

	std::shared_ptr<std::vector<std::vector<int>>>     m_bad_groups_cache;
	std::recursive_mutex                               m_bad_groups_mutex;

	std::shared_ptr<std::map<int, std::vector<int>>>   m_symmetric_groups_cache;
	std::recursive_mutex                               m_symmetric_groups_mutex;

	std::shared_ptr<metabalancer_groups_info_t>        m_metabalancer_groups_info;
	std::recursive_mutex                               m_weight_cache_mutex;

	const int                                          m_group_info_update_period;
	std::thread                                        m_weight_cache_update_thread;
	std::condition_variable                            m_weight_cache_condition_variable;
	std::mutex                                         m_mutex;
	bool                                               m_done;
	std::mutex                                         m_reconnect_mutex;

	std::shared_ptr<std::map<std::string, std::vector<int>>> m_cache_groups;
	std::recursive_mutex                               m_cache_groups_mutex;

	std::shared_ptr<cocaine::framework::app_service_t> m_app;
	std::shared_ptr<cocaine::framework::service_manager_t> m_service_manager;
};

template <typename F, typename R, typename... Args>
void mastermind_t::data::retry(F func, R &res, Args &&... args) {
	bool already_tried = false;
	try {
		if (!m_service_manager || !m_app || m_app->status() != cocaine::framework::service_status::connected) {
			COCAINE_LOG_INFO(m_logger, "libmastermind: retry: preconnect");
			already_tried = true;
			reconnect();
		}
		auto g = (m_app.get()->*func)(std::forward<Args>(args)...);
		g.wait_for(std::chrono::seconds(4));
		if (g.ready() == false) {
			if (already_tried) {
				throw std::runtime_error("cannot process enqueue");
			}
			COCAINE_LOG_INFO(m_logger, "libmastermind: retry: try to reconnect");
			reconnect();
			auto g = (m_app.get()->*func)(std::forward<Args>(args)...);
			g.wait_for(std::chrono::seconds(4));
			if (g.ready() == false) {
				throw std::runtime_error("cannot reprocess enqueue");
			}
			auto chunk = g.next();
			res = cocaine::framework::unpack<R>(chunk);
		}
		auto chunk = g.next();
		res = cocaine::framework::unpack<R>(chunk);
	} catch (const cocaine::framework::service_error_t &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: retry: %s", ex.what());
		throw;
	} catch (const std::exception &ex) {
		COCAINE_LOG_ERROR(m_logger, "libmastermind: retry: %s", ex.what());
		throw;
	}
}

} // namespace mastermind

#endif /* SRC__MASTERMIND_IMPL_HPP */
