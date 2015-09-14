#include "namespace_state_p.hpp"
#include "utils.hpp"

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cstdlib>
#include <sstream>

mastermind::namespace_state_t::user_settings_t::~user_settings_t() {
}

mastermind::namespace_state_t::data_t::settings_t::settings_t(const std::string &name
		, const kora::config_t &state , const user_settings_factory_t &factory)
	try
	: groups_count(state.at<size_t>("groups-count"))
	, success_copies_num(state.at<std::string>("success-copies-num"))
{
	if (state.has("auth-keys")) {
		const auto &auth_keys_state = state.at("auth-keys");

		auth_keys.read = auth_keys_state.at<std::string>("read", "");
		auth_keys.read = auth_keys_state.at<std::string>("write", "");
	}

	if (state.has("static-couple")) {
		const auto &static_couple_config = state.at("static-couple");

		for (size_t index = 0, size = static_couple_config.size(); index != size; ++index) {
			static_groups.emplace_back(static_couple_config.at<group_t>(index));
		}
	}

	if (factory) {
		user_settings_ptr = factory(name, state);
	}
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create settings-state: ") + ex.what());
}

mastermind::namespace_state_t::data_t::settings_t::settings_t(settings_t &&other)
	: groups_count(other.groups_count)
	, success_copies_num(std::move(other.success_copies_num))
	, auth_keys(std::move(other.auth_keys))
	, user_settings_ptr(std::move(other.user_settings_ptr))
{
}

mastermind::namespace_state_t::data_t::couples_t::couples_t(const kora::config_t &state)
	try
{
	for (size_t index = 0, size = state.size(); index != size; ++index) {
		const auto &couple_info_state = state.at(index);

		auto couple_id = couple_info_state.at<std::string>("id");

		auto ci_insert_result = couple_info_map.insert(std::make_pair(
					couple_id, couple_info_t()));

		if (std::get<1>(ci_insert_result) == false) {
			throw std::runtime_error("reuse the same couple_id=" + couple_id);
		}

		auto &couple_info = std::get<0>(ci_insert_result)->second;

		couple_info.id = couple_id;

		{
			const auto &dynamic_tuple = couple_info_state.at("tuple")
				.underlying_object().as_array();

			for (auto it = dynamic_tuple.begin(), end = dynamic_tuple.end();
					it != end; ++it) {
				couple_info.groups.emplace_back(it->to<group_t>());
			}
		}

		{
			auto status = couple_info_state.at<std::string>("couple_status");

			if (status == "BAD") {
				couple_info.status = couple_info_t::status_tag::BAD;
			} else {
				couple_info.status = couple_info_t::status_tag::UNKNOWN;
			}
		}

		couple_info.free_effective_space = couple_info_state.at<uint64_t>("free_effective_space", 0);

		couple_info.hosts = couple_info_state.at("hosts").underlying_object();

		const auto &groups_info_state = couple_info_state.at("groups");

		for (size_t index = 0, size = groups_info_state.size(); index != size; ++index) {
			const auto &group_info_state = groups_info_state.at(index);

			auto group_id = group_info_state.at<group_t>("id");

			auto gi_insert_result = group_info_map.insert(std::make_pair(
						group_id, group_info_t()));

			if (std::get<1>(gi_insert_result) == false) {
				throw std::runtime_error("resuse the same group_id="
						+ boost::lexical_cast<std::string>(group_id));
			}

			auto &group_info = std::get<0>(gi_insert_result)->second;

			group_info.id = group_id;

			{
				auto status = group_info_state.at<std::string>("status");

				if (status == "COUPLED") {
					group_info.status = group_info_t::status_tag::COUPLED;
				} else {
					group_info.status = group_info_t::status_tag::UNKNOWN;
				}
			}

			group_info.couple_info_map_iterator = std::get<0>(ci_insert_result);
			couple_info.groups_info_map_iterator.emplace_back(std::get<0>(gi_insert_result));
		}
	}
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create couples-state: ") + ex.what());
}

mastermind::namespace_state_t::data_t::couples_t::couples_t(couples_t &&other)
	: group_info_map(std::move(other.group_info_map))
	, couple_info_map(std::move(other.couple_info_map))
{
}

mastermind::namespace_state_t::data_t::statistics_t::statistics_t(const kora::config_t &config)
	try
	: is_full(config.at("is_full", false))
{
	// TODO: log whether is_full is provided by mastermind
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create statistics-state: ") + ex.what());
}

bool
mastermind::namespace_state_t::data_t::statistics_t::ns_is_full() const {
	return is_full;
}

mastermind::namespace_state_t::data_t::data_t(std::string name_, const kora::config_t &config
		, const user_settings_factory_t &factory)
	try
	: name(std::move(name_))
	, settings(name, config.at("settings"), factory)
	, couples(config.at("couples"))
	, weights(config.at("weights"), settings.groups_count, !settings.static_groups.empty())
	, statistics(config.at("statistics"))
{
	check_consistency();

} catch (const std::exception &ex) {
	throw std::runtime_error("cannot create ns-state " + name + ": " + ex.what());
}

mastermind::namespace_state_t::data_t::data_t(data_t &&other)
	: name(std::move(other.name))
	, settings(std::move(other.settings))
	, couples(std::move(other.couples))
	, weights(std::move(other.weights))
	, statistics(std::move(other.statistics))
	, extract(std::move(other.extract))
{
}

void
mastermind::namespace_state_t::data_t::check_consistency() {
	std::ostringstream oss;

	oss << "namespace=" << name;
	oss << " groups-count=" << settings.groups_count;

	{
		if (couples.couple_info_map.empty()) {
			throw std::runtime_error("couples list is empty");
		}
	}

	{
		size_t nonzero_weights = 0;

		const auto &weights_data = weights.data();

		for (auto it = weights_data.begin(), end = weights_data.end(); it != end; ++it) {
			auto weight = it->weight;

			if (weight != 0) {
				nonzero_weights += 1;
			}

			{
				auto couple_it = couples.couple_info_map.cend();
				const auto &groups = it->groups;

				if (groups.size() != settings.groups_count) {
					std::ostringstream oss;
					oss
						<< "groups.size is not equal to groups_count(" << settings.groups_count
						<< "), groups=" << groups;
					throw std::runtime_error(oss.str());
				}

				for (auto git = groups.begin(), gend = groups.end(); git != gend; ++git) {
					auto group_info_it = couples.group_info_map.find(*git);

					if (group_info_it == couples.group_info_map.end()) {
						throw std::runtime_error("there is no group-info for group "
								+ boost::lexical_cast<std::string>(*git));
					}

					if (couple_it != couples.couple_info_map.end()) {
						if (couple_it != group_info_it->second.couple_info_map_iterator) {
							std::ostringstream oss;
							oss << "inconsisten couple: " << groups;
							throw std::runtime_error(oss.str());
						}
					} else {
						couple_it = group_info_it->second.couple_info_map_iterator;
					}
				}

				if (couple_it->second.groups.size() != groups.size()) {
					std::ostringstream oss;
					oss << "inconsisten couple: " << groups;
					throw std::runtime_error(oss.str());
				}
			}
		}

		bool is_static = false;

		if (nonzero_weights == 0 && !statistics.ns_is_full()) {
			if (settings.static_groups.empty()) {
				throw std::runtime_error("no weighted couples were obtained from mastermind");
			} else {
				// Because namespace has static couple
				nonzero_weights = 1;
				is_static = true;
			}
		}

		oss << " couples-for-write=" << nonzero_weights;

		if (is_static) {
			oss << " [static]";
		}

		if (statistics.ns_is_full()) {
			oss << " [full]";
		}
	}

	oss << " couples=" << couples.couple_info_map.size();

	extract = oss.str();
}

mastermind::namespace_state_init_t::namespace_state_init_t(
		std::shared_ptr<namespace_state_t::data_t> data_) {
	data = std::move(data_);
}

mastermind::namespace_state_init_t::data_t::data_t(std::string name
		, const kora::config_t &config, const user_settings_factory_t &factory)
	: namespace_state_t::data_t(std::move(name), config, factory)
{
}

mastermind::namespace_state_init_t::data_t::data_t(data_t &&other)
	: namespace_state_t::data_t(std::move(other))
{
}

