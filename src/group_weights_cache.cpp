#include <map>
#include <vector>
#include <sstream>

#include <boost/thread.hpp>

#include "utils.hpp"

namespace elliptics {

namespace {
class weighted_groups {
public:
	weighted_groups();
	void add(const std::vector<int> &group, uint64_t weight);
	const std::vector<int> &get() const;
	const std::map<uint64_t, std::vector<int> > &data() const { return map_; }
private:
	uint64_t total_weight_;
	std::map<uint64_t, std::vector<int> > map_;
};

weighted_groups::weighted_groups() :
		total_weight_(0)
{}

void weighted_groups::add(const std::vector<int> &group, uint64_t weight)
{
	total_weight_ += weight;
	map_[total_weight_] = group;
}

const std::vector<int> &weighted_groups::get() const {
	double shoot_point = double(random()) / RAND_MAX * total_weight_;
	std::map<uint64_t, std::vector<int> >::const_iterator it = map_.lower_bound(uint64_t(shoot_point));
	if (it == map_.end()) {
		throw std::runtime_error("Could not find anything");
	}
	return it->second;
}

}

class group_weights_cache_impl : public group_weights_cache_interface_t {
public:
	virtual ~group_weights_cache_impl();
	virtual bool update(metabase_group_weights_response_t &resp);
	virtual std::vector<int> choose(uint64_t count, const std::string &name_space);
	virtual bool initialized();
	virtual std::string to_string();
private:
	std::map<std::string, std::map<uint64_t, weighted_groups>> map_;
};

group_weights_cache_impl::~group_weights_cache_impl()
{}

bool group_weights_cache_impl::update(metabase_group_weights_response_t &resp) {
	std::map<std::string, std::map<uint64_t, weighted_groups>> local_map;

	for (auto nt = resp.info.begin(); nt != resp.info.end(); ++nt) {
		std::map<uint64_t, weighted_groups> local_map2;
		typedef std::vector<metabase_group_weights_response_t::SizedGroups>::const_iterator resp_iterator;
		resp_iterator e = nt->sized_groups.end();
		for (resp_iterator it = nt->sized_groups.begin(); it != e; ++it) {
			typedef std::vector<metabase_group_weights_response_t::GroupWithWeight>::const_iterator info_iterator;
			weighted_groups groups;
			info_iterator wg_e = it->weighted_groups.end();
			for (info_iterator wg_it = it->weighted_groups.begin(); wg_it != wg_e; ++wg_it) {
				groups.add(wg_it->group_ids, wg_it->weight);
			}
			std::swap(local_map2[it->size], groups);
		}
		std::swap(local_map[nt->name], local_map2);
	}
	if (!local_map.empty()) {
		map_.swap(local_map);
		return true;
	}
	return false;
}

std::vector<int> group_weights_cache_impl::choose(uint64_t count, const std::string &name_space) {
	auto it = map_.find(name_space);
	if (it == map_.end()) {
		std::ostringstream oss;
		oss << "libmastermind cannot find namespace \'" << name_space << "\' in group_weights_cache";
		throw std::runtime_error(oss.str());
	}
	return it->second[count].get();
}

bool group_weights_cache_impl::initialized() {
	return !map_.empty();
}

std::string group_weights_cache_impl::to_string() {
	std::ostringstream oss;
	oss << "{" << std::endl;
	for (auto it = map_.begin(), ite = --map_.end(); it != map_.end(); ++it) {
		oss << "\t\"" << it->first << "\" : {" << std::endl;

		for (auto it2 = it->second.begin(), it2e = --it->second.end(); it2 != it->second.end(); ++it2) {
			oss << "\t\t\"" << it2->first << "\" : {" << std::endl;

			for (auto it3 = it2->second.data().begin(), it3e = --it2->second.data().end(); it3 != it2->second.data().end(); ++it3) {
				oss << "\t\t\t\"" << it3->first << "\" : [";
				for (auto it4 = it3->second.begin(), it4e = --it3->second.end(); it4 != it3->second.end(); ++it4) {
					oss << *it4;
					if (it4 != it4e) oss << ", ";
				}
				oss << ']';
				if (it3 != it3e) {
					oss << ',';
				}
				oss << std::endl;
			}

			oss << "\t\t}";
			if (it2 != it2e) {
				oss << ',';
			}
			oss << std::endl;
		}

		oss << "\t}";
		if (it != ite) {
			oss << ',';
		}
		oss << std::endl;
	}
	oss << "}";
	return oss.str();
}

std::shared_ptr<group_weights_cache_interface_t> get_group_weighs_cache() {
	return std::make_shared<group_weights_cache_impl>();
}

}

