#include <map>
#include <vector>

#include <boost/thread.hpp>

#include "elliptics/proxy.hpp"

#ifdef HAVE_METABASE

namespace elliptics {

namespace {
class weighted_groups {
public:
    weighted_groups();
    void add(const std::vector<int> &group, uint64_t weight);
    const std::vector<int> &get() const;
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
    if(it == map_.end()) {
        throw std::runtime_error("Could not find anything");
    }
    return it->second;
}

}

class group_weights_cache_impl : public group_weights_cache_interface {
public:
    virtual ~group_weights_cache_impl();
    virtual bool update(MetabaseGroupWeightsResponse &resp);
    virtual std::vector<int> choose(uint64_t count);
    virtual bool initialized();
private:
    boost::shared_mutex shared_mutex_;
    std::map<uint64_t, weighted_groups> map_;
};

group_weights_cache_impl::~group_weights_cache_impl()
{}

bool group_weights_cache_impl::update(MetabaseGroupWeightsResponse &resp) {
    std::map<uint64_t, weighted_groups> local_map;
    typedef std::vector<MetabaseGroupWeightsResponse::SizedGroups>::const_iterator resp_iterator;
    resp_iterator e = resp.info.end();
    for(resp_iterator it = resp.info.begin(); it != e; ++it) {
        typedef std::vector<MetabaseGroupWeightsResponse::GroupWithWeight>::const_iterator info_iterator;
        weighted_groups groups;
        info_iterator wg_e = it->weighted_groups.end();
        for(info_iterator wg_it = it->weighted_groups.begin(); wg_it != wg_e; ++wg_it) {
            groups.add(wg_it->group_ids, wg_it->weight);
        }
        std::swap(local_map[it->size], groups);
    }
    boost::unique_lock<boost::shared_mutex> lock(shared_mutex_);
    if(!local_map.empty()) {
        map_.swap(local_map);
        return true;
    }
    return false;
}

std::vector<int> group_weights_cache_impl::choose(uint64_t count) {
    boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
    return map_[count].get();
}

bool group_weights_cache_impl::initialized() {
    boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
    return !map_.empty();
}

std::auto_ptr<group_weights_cache_interface> get_group_weighs_cache() {
    return std::auto_ptr<group_weights_cache_interface>(new group_weights_cache_impl);
}

}

#endif
