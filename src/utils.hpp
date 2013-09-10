#ifndef SRC__UTILS_HPP
#define SRC__UTILS_HPP

#include <msgpack.hpp>

namespace elliptics {

struct metabase_group_weights_request_t {
  uint64_t stamp;
  MSGPACK_DEFINE(stamp)
};

struct metabase_group_weights_response_t {
  struct GroupWithWeight {
	std::vector<int> group_ids;
	uint64_t weight;
	MSGPACK_DEFINE(group_ids, weight)
  };
  struct SizedGroups {
	uint64_t size;
	std::vector<GroupWithWeight> weighted_groups;
	MSGPACK_DEFINE(size, weighted_groups)
  };
  struct NamedGroups {
	std::string name;
	std::vector<SizedGroups> sized_groups;
	MSGPACK_DEFINE(name, sized_groups)
  };
  std::vector<NamedGroups> info;
  MSGPACK_DEFINE(info)
};

class group_weights_cache_interface_t {
public:
  virtual ~group_weights_cache_interface_t() {};

  virtual bool update(metabase_group_weights_response_t &resp) = 0;
  virtual std::vector<int> choose(uint64_t count, const std::string &name_space) = 0;
  virtual bool initialized() = 0;
};

std::unique_ptr<group_weights_cache_interface_t> get_group_weighs_cache();

enum GROUP_INFO_STATUS {
  GROUP_INFO_STATUS_OK,
  GROUP_INFO_STATUS_BAD,
  GROUP_INFO_STATUS_COUPLED
};

struct group_info_request_t {
  int group;
  MSGPACK_DEFINE(group)
};

}

#endif /* SRC__UTILS_HPP */
