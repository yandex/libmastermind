#ifndef _UTILS_FASTCGI_HPP_INCLUDED_
#define _UTILS_FASTCGI_HPP_INCLUDED_

#include "elliptics/proxy.hpp"

namespace elliptics {

enum METABASE_TYPE {
  PROXY_META_NONE = 0,
  PROXY_META_OPTIONAL,
  PROXY_META_NORMAL,
  PROXY_META_MANDATORY
};

#ifdef HAVE_METABASE
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
  std::vector<SizedGroups> info;
  MSGPACK_DEFINE(info)
};

class group_weights_cache_interface_t {
public:
  virtual ~group_weights_cache_interface_t() {};

  virtual bool update(metabase_group_weights_response_t &resp) = 0;
  virtual std::vector<int> choose(uint64_t count) = 0;
  virtual bool initialized() = 0;
};

std::auto_ptr<group_weights_cache_interface_t> get_group_weighs_cache();

enum GROUP_INFO_STATUS {
  GROUP_INFO_STATUS_OK,
  GROUP_INFO_STATUS_BAD,
  GROUP_INFO_STATUS_COUPLED
};

struct group_info_request_t {
  int group;
  MSGPACK_DEFINE(group)
};
#endif /* HAVE_METABASE */

struct dnet_id_less {
  bool operator () (const struct dnet_id &ob1, const struct dnet_id &ob2);
};

}

#endif /* _UTILS_FASTCGI_HPP_INCLUDED_ */
