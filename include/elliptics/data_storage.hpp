#ifndef _ELLIPTICS_DATA_STORAGE_HPP_
#define _ELLIPTICS_DATA_STORAGE_HPP_

#include <map>
#include <string>
#include <sstream>
#include <boost/optional.hpp>
#include <msgpack.hpp>
#include <elliptics/cppdef.h>

namespace elliptics {

template<typename T>
struct type_traits_base {
	typedef T type;
	static ioremap::elliptics::data_pointer convert(const type &ob) {
		ioremap::elliptics::data_buffer data_buffer;
		msgpack::pack(data_buffer, ob);
		return std::move(data_buffer);
	}

	static type convert(ioremap::elliptics::data_pointer data_pointer) {
		type res;
		msgpack::unpacked unpacked;
		msgpack::unpack(&unpacked, (const char *)data_pointer.data(), data_pointer.size());
		unpacked.get().convert(&res);
		return res;
	}
};

template<size_t type>
struct type_traits;

template<> struct type_traits<0> : type_traits_base<time_t> {};

class data_storage_t {
public:
	data_storage_t() {}

	data_storage_t(const std::string &message)
		: data(message)
	{}

	data_storage_t(std::string &&message)
		: data(std::move(message))
	{}

	data_storage_t(const data_storage_t &ds)
		: data(ds.data)
		, embeds(ds.embeds)
	{}

	data_storage_t(data_storage_t &&ds)
		: data(std::move(ds.data))
		, embeds(std::move(ds.embeds))
	{}

	data_storage_t &operator = (const data_storage_t &ds) {
		data = ds.data;
		embeds = ds.embeds;
		return *this;
	}

	data_storage_t &operator = (data_storage_t &&ds) {
		data = std::move(ds.data);
		embeds = std::move(ds.embeds);
		return *this;
	}

	template<size_t type>
	boost::optional<typename type_traits<type>::type> get() const {
		auto it = embeds.find(type);
		if (it == embeds.end())
			return boost::optional<typename type_traits<type>::type>();
		return type_traits<type>::convert(it->second.data_pointer);
	}

	template<size_t type>
	void set(const typename type_traits<type>::type &ob) {
		embed_t e;
		e.flags = 0;
		e.data_pointer = type_traits<type>::convert(ob);
		embeds.insert(std::make_pair(type, e));
	}

	static ioremap::elliptics::data_pointer pack(const data_storage_t &ds);
	static data_storage_t unpack(ioremap::elliptics::data_pointer data_pointer, bool embeded = false);

	std::string data;

private:
	struct embed_t {
		size_t flags;
		ioremap::elliptics::data_pointer data_pointer;
	};

	std::map<size_t, embed_t> embeds;
};
} // namespace elliptcis

#endif /* _ELLIPTICS_DATA_STORAGE_HPP_ */
