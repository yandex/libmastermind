#include "namespace_state_p.hpp"

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cstdlib>

mastermind::namespace_state_t::user_settings_t::~user_settings_t() {
}

mastermind::namespace_state_t::data_t::settings_t::settings_t(const kora::config_t &state
		, const user_settings_factory_t &factory)
	try
	: groups_count(state.at<size_t>("groups-count"))
	, success_copies_num(state.at<std::string>("success-copies-num"))
{
	if (state.has("auth-keys")) {
		const auto &auth_keys_state = state.at("auth-keys");

		auth_keys.read = auth_keys_state.at<std::string>("read", "");
		auth_keys.read = auth_keys_state.at<std::string>("write", "");
	}

	if (factory) {
		user_settings_ptr = factory(state);
	}
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create settings-state: ") + ex.what());
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

		couple_info.free_effective_space = couple_info_state.at<uint64_t>("free_effective_space");

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

mastermind::namespace_state_t::data_t::weights_t::weights_t(const kora::config_t &state
		, size_t groups_count_)
	try
	: groups_count(groups_count_)
{
	const auto &couples = state.at(boost::lexical_cast<std::string>(groups_count))
		.underlying_object().as_array();

	couples_with_info_t couples_with_info;

	for (auto it = couples.begin(), end = couples.end(); it != end; ++it) {
		const auto &couple = it->as_array();

		groups_t groups;

		{
			const auto &dynamic_groups = couple[0].as_array();

			for (auto it = dynamic_groups.begin(), end = dynamic_groups.end();
					it != end; ++it) {
				groups.emplace_back(it->to<group_t>());
			}
		}

		couples_with_info.emplace_back(std::make_tuple(groups
					, couple[1].to<uint64_t>(), couple[2].to<uint64_t>()));
	}

	set(std::move(couples_with_info));
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create weights-state: ") + ex.what());
}


void
mastermind::namespace_state_t::data_t::weights_t::set(couples_with_info_t couples_with_info_)
{
	couples_with_info = std::move(couples_with_info_);

	std::sort(couples_with_info.begin(), couples_with_info.end(), couples_with_info_cmp);

	for (size_t index = 0; index != couples_with_info.size(); ++index) {
		auto avalible_memory = std::get<2>(couples_with_info[index]);
		uint64_t total_weight = 0;

		for (size_t index2 = 0; index2 <= index; ++index2) {
			const auto &couple_with_info = couples_with_info[index2];

			auto weight = std::get<1>(couple_with_info);

			if (weight == 0) {
				continue;
			}

			total_weight += weight;

			couples_by_avalible_memory[avalible_memory].insert(
				std::make_pair(total_weight, std::cref(couple_with_info))
			);
		}
	}
}

mastermind::namespace_state_t::data_t::weights_t::couple_with_info_t
mastermind::namespace_state_t::data_t::weights_t::get(size_t groups_count_
		, uint64_t size) const {
	if (groups_count_ != groups_count) {
		throw invalid_groups_count_error();
	}

	auto amit = couples_by_avalible_memory.lower_bound(size);
	if (amit == couples_by_avalible_memory.end()) {
		throw not_enough_memory_error();
	}

	auto &weighted_groups = amit->second;
	if (weighted_groups.empty()) {
		throw couple_not_found_error();
	}

	auto total_weight = weighted_groups.rbegin()->first;
	double shoot_point = double(random()) / RAND_MAX * total_weight;
	auto it = weighted_groups.lower_bound(uint64_t(shoot_point));

	if (it == weighted_groups.end()) {
		throw couple_not_found_error();
	}

	return it->second;
}

const mastermind::namespace_state_t::data_t::weights_t::couples_with_info_t &
mastermind::namespace_state_t::data_t::weights_t::data() const {
	return couples_with_info;
}

bool
mastermind::namespace_state_t::data_t::weights_t::couples_with_info_cmp(
		const couple_with_info_t &lhs, const couple_with_info_t &rhs) {
	return std::get<2>(lhs) > std::get<2>(rhs);
}


mastermind::namespace_state_t::data_t::statistics_t::statistics_t(const kora::config_t &state)
	try
{
} catch (const std::exception &ex) {
	throw std::runtime_error(std::string("cannot create statistics-state: ") + ex.what());
}

mastermind::namespace_state_t::data_t::data_t(std::string name_, const kora::config_t &config
		, const user_settings_factory_t &factory)
	try
	: name(std::move(name_))
	, settings(config.at("settings"), factory)
	, couples(config.at("couples"))
	, weights(config.at("weights"), settings.groups_count)
	, statistics(config.at("statistics"))
{
	// TODO: check consistency
	// if (<not consistency>) {
	// 	throw std::runtime_error("state is not consistent");
	// }

	// TODO: log ns-state extract
} catch (const std::exception &ex) {
	throw std::runtime_error("cannot create ns-state " + name + ": " + ex.what());
}

mastermind::namespace_state_t::namespace_state_t() {
}

mastermind::namespace_state_init_t::namespace_state_init_t(
		std::shared_ptr<const namespace_state_t::data_t> data_) {
	data = std::move(data_);
}

mastermind::namespace_state_init_t::data_t::data_t(std::string name
		, const kora::config_t &config, const user_settings_factory_t &factory)
	: namespace_state_t::data_t(std::move(name), config, factory)
{
}

