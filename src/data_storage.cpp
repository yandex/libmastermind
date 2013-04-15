#include "elliptics/data_storage.hpp"

namespace elliptics {
namespace details {
template<>
void bwrite(std::ostringstream &oss, std::string str) {
	oss.write(str.data(), str.size());
}

void bread(std::istringstream &iss, std::string &str, size_t size) {
	str.resize(size);
	iss.read(const_cast<char *>(str.data()), size);
}
} // namespace details

std::string data_storage::pack(const data_storage &ds) {
	using namespace details;
	std::ostringstream oss;
	size_t embeds_size = sizeof(size_t);
	for(auto it = ds.embeds.begin(); it != ds.embeds.end(); ++it) {
		embeds_size += it->second.size() + 2 * sizeof(size_t);
	}

	if(embeds_size != 0)
		bwrite(oss, embeds_size);

	for(auto it = ds.embeds.begin(); it != ds.embeds.end(); ++it) {
		bwrite(oss, it->first);
		bwrite(oss, it->second.size());
		bwrite(oss, it->second);
	}

	bwrite(oss, ds.data);
	return oss.str();
}

data_storage data_storage::unpack(const std::string &message, bool embeded) {
	using namespace details;
	elliptics::data_storage ds;
	std::istringstream iss(message);
	size_t embeds_size = 0;

	if (embeded) {
		bread(iss, embeds_size);
		embeds_size -= sizeof(size_t);
		size_t read_size = 0;
		while(read_size < embeds_size) {
			size_t type;
			size_t size;
			std::string embed;

			bread(iss, type);
			bread(iss, size);
			bread(iss, embed, size);

			read_size += size + 2 * sizeof(size_t);
			ds.embeds.insert(std::make_pair(type, std::move(embed)));
		}
	}

	size_t data_size = message.size() - embeds_size;
	bread(iss, ds.data, data_size);
	return ds;
}
} // namespace elliptics
