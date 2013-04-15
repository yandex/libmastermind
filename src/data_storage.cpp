#include "elliptics/data_storage.hpp"

namespace {
template<typename T>
void read(ioremap::elliptics::data_pointer &data_pointer, T&ob) {
	ob = *data_pointer.data<T>();
	data_pointer = data_pointer.skip<T>();
}
}

namespace elliptics {
ioremap::elliptics::data_pointer data_storage_t::pack(const data_storage_t &ds) {
	size_t embeds_size = 0;
	for(auto it = ds.embeds.begin(); it != ds.embeds.end(); ++it) {
		embeds_size += it->second.data_pointer.size() + 3 * sizeof(size_t);
	}

	size_t total_size = ds.data.size();
	if (embeds_size != 0)
		total_size += embeds_size + sizeof(size_t);

	ioremap::elliptics::data_buffer data(total_size);

	if (embeds_size != 0)
		data.write<size_t>(embeds_size);

	for(auto it = ds.embeds.begin(); it != ds.embeds.end(); ++it) {
		data.write(it->first);
		data.write(it->second.flags);
		data.write(it->second.data_pointer.size());
		data.write((const char *)it->second.data_pointer.data(), it->second.data_pointer.size());
	}

	data.write(ds.data.data(), ds.data.size());
	return std::move(data);
}

data_storage_t data_storage_t::unpack(ioremap::elliptics::data_pointer data_pointer, bool embeded) {
	elliptics::data_storage_t ds;
	size_t embeds_size = 0;
	size_t data_size = data_pointer.size();

	if (embeded) {
		embeds_size = *data_pointer.data<size_t>();
		data_pointer = data_pointer.skip<size_t>();
		size_t read_size = 0;
		while(read_size < embeds_size) {
			embed_t e;
			size_t type;
			size_t size;

			read(data_pointer, type);
			read(data_pointer, e.flags);
			read(data_pointer, size);
			e.data_pointer = data_pointer.slice(0, size);
			data_pointer = data_pointer.skip(size);

			read_size += size + 3 * sizeof(size_t);
			ds.embeds.insert(std::make_pair(type, e));
		}
	}

	if (embeded)
		data_size-= embeds_size + sizeof(size_t);
	ds.data.assign((char *)data_pointer.data(), data_pointer.size());
	return ds;
}
} // namespace elliptics
