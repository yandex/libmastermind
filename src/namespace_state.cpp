#include "namespace_state_p.hpp"

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cstdlib>

mastermind::namespace_state_t::user_settings_t::~user_settings_t() {
}

mastermind::namespace_state_t::data_t::settings_t
mastermind::namespace_state_t::data_t::settings_t::create(const kora::config_t &state
		, const user_settings_factory_t &factory) {
	try {
		settings_t settings;

		settings.groups_count = state.at<size_t>("groups-count");
		settings.success_copies_num = state.at<std::string>("success-copies-num");

		if (state.has("auth-keys")) {
			const auto &auth_keys_state = state.at("auth-keys");

			settings.auth_keys.read = auth_keys_state.at<std::string>("read", "");
			settings.auth_keys.read = auth_keys_state.at<std::string>("write", "");
		}

		if (factory) {
			settings.user_settings_ptr = factory(state);
		}

		return settings;
	} catch (const std::exception &ex) {
		throw std::runtime_error(std::string("cannot create settings-state: ") + ex.what());
	}
}

mastermind::namespace_state_t::data_t::couples_t
mastermind::namespace_state_t::data_t::couples_t::create(const kora::config_t &state) {
	try {
		couples_t couples;

		for (size_t index = 0, size = state.size(); index != size; ++index) {
			const auto &couple_info_state = state.at(index);

			auto couple_id = couple_info_state.at<std::string>("id");

			auto ci_insert_result = couples.couple_info_map.insert(std::make_pair(
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

			const auto &groups_info_state = state.at("groups");

			for (size_t index = 0, size = groups_info_state.size(); index != size; ++index) {
				const auto &group_info_state = groups_info_state.at(index);

				auto group_id = group_info_state.at<group_t>("id");

				auto gi_insert_result = couples.group_info_map.insert(std::make_pair(
							group_id, group_info_t()));

				if (std::get<1>(gi_insert_result) == false) {
					throw std::runtime_error("resuse the same group_id="
							+ boost::lexical_cast<std::string>(group_id));
				}

				auto &group_info = std::get<0>(gi_insert_result)->second;

				group_info.id = group_id;
				group_info.couple_info_map_iterator = std::get<0>(ci_insert_result);

				couple_info.groups_info_map_iterator.emplace_back(std::get<0>(gi_insert_result));
			}
		}

		return couples;
	} catch (const std::exception &ex) {
		throw std::runtime_error(std::string("cannot create couples-state: ") + ex.what());
	}
}

mastermind::namespace_state_t::data_t::weights_t
mastermind::namespace_state_t::data_t::weights_t::create(const kora::config_t &state
		, size_t groups_count) {
	try {
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

		weights_t weights;

		weights.set(std::move(couples_with_info));

		return weights;
	} catch (const std::exception &ex) {
		throw std::runtime_error(std::string("cannot create weights-state: ") + ex.what());
	}
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
mastermind::namespace_state_t::data_t::weights_t::get(uint64_t size) const {
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

bool
mastermind::namespace_state_t::data_t::weights_t::couples_with_info_cmp(
		const couple_with_info_t &lhs, const couple_with_info_t &rhs) {
	return std::get<2>(lhs) > std::get<2>(rhs);
}


mastermind::namespace_state_t::data_t::statistics_t
mastermind::namespace_state_t::data_t::statistics_t::create(const kora::config_t &state) {
	try {
		statistics_t statistics;

		return statistics;
	} catch (const std::exception &ex) {
		throw std::runtime_error(std::string("cannot create statistics-state: ") + ex.what());
	}
}

std::shared_ptr<mastermind::namespace_state_t::data_t>
mastermind::namespace_state_t::data_t::create(std::string name
		, const kora::config_t &state , const user_settings_factory_t &factory) {
	auto data = std::make_shared<data_t>();

	data->name = std::move(name);

	try {
		data->settings = data_t::settings_t::create(state.at("settings"), factory);
		data->couples = couples_t::create(state.at("couples"));
		data->weights = weights_t::create(state.at("weights"), data->settings.groups_count);
		data->statistics = statistics_t::create(state.at("statistics"));

		// TODO: check consistency
		// if (<not consistency>) {
		// 	throw std::runtime_error("state is not consistent");
		// }

	} catch (const std::exception &ex) {
		throw std::runtime_error("cannot create ns-state " + data->name + ": " + ex.what());
	}

	return data;
}

mastermind::namespace_state_t::namespace_state_t() {
}

mastermind::namespace_state_init_t::namespace_state_init_t(std::shared_ptr<const data_t> data_) {
	data = std::move(data_);
}

