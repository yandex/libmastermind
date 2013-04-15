#ifndef __READ_RESULT__
#define __READ_RESULT__

#include <map>
#include <string>
#include <sstream>
#include <boost/optional.hpp>
#include <msgpack.hpp>

namespace elliptics {

template<typename T>
struct type_traits_base {
	typedef T type;
	static std::string convert(const type &ob) {
		msgpack::sbuffer buffer;
		msgpack::pack(buffer, ob);
		return buffer.data();
	}

	static type convert(const std::string &ob) {
		type res;
		msgpack::unpacked unpacked;
		msgpack::unpack(&unpacked, ob.data(), ob.size());

		unpacked.get().convert(&res);
		return res;
	}
};

template<size_t type>
struct type_traits;

template<> struct type_traits<0> : type_traits_base<time_t> {};

namespace details {
template<typename T>
void bwrite(std::ostringstream &oss, T ob) {
	oss.write((const char *)&ob, sizeof(T));
}

template<typename T>
void bread(std::istringstream &iss, T &ob) {
	iss.read((char *)&ob, sizeof(T));
}

void bread(std::istringstream &iss, std::string &str, size_t size);
} // namespace details

class data_storage {
public:
	data_storage() {}

	data_storage(const std::string &message)
		: data(message)
	{}

	data_storage(std::string &&message)
		:data(std::move(message))
	{}

	data_storage(const data_storage &ds)
		: data(ds.data)
		, embeds(ds.embeds)
	{}

	data_storage(data_storage &&ds)
		: data(std::move(ds.data))
		, embeds(std::move(ds.embeds))
	{}

	data_storage &operator = (const data_storage &ds) {
		data = ds.data;
		embeds = ds.embeds;
		return *this;
	}

	data_storage &operator = (data_storage &&ds) {
		data = std::move(ds.data);
		embeds = std::move(ds.embeds);
		return *this;
	}

	template<size_t type>
	boost::optional<typename type_traits<type>::type> get() const {
		auto it = embeds.find(type);
		if (it == embeds.end())
			return boost::optional<typename type_traits<type>::type>();
		return type_traits<type>::convert(it->second);
	}

	template<size_t type>
	void set(const typename type_traits<type>::type &ob) {
		embeds.insert(std::make_pair(type,
									type_traits<type>::convert(ob)));
	}

	static std::string pack(const data_storage &ds);
	static data_storage unpack(const std::string &message, bool embeded = false);

	std::string data;

private:
	std::map<size_t, std::string> embeds;
};
} // namespace elliptcis

#endif /* __READ_RESULT__ */
