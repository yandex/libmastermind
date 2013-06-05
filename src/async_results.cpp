#include "elliptics/async_results.hpp"
#include "elliptics/proxy.hpp"

elliptics::async_read_result_t::async_read_result_t(elliptics::async_read_result_t::inner_result_t &&inner_result, bool embeded)
	: m_inner_result(std::move(inner_result))
	, m_embeded(embeded)
{
}

elliptics::async_read_result_t::async_read_result_t(async_read_result_t &&ob)
	: m_inner_result(std::move(ob.m_inner_result))
	, m_embeded(ob.m_embeded)
{
}

std::vector<elliptics::async_read_result_t::outer_result_t> elliptics::async_read_result_t::get()
{
	auto v = m_inner_result.get();
	std::vector<outer_result_t> res;
	res.reserve(v.size());

	for (auto it = v.begin(); it != v.end(); ++it) {
		res.push_back(data_container_t::unpack(it->file(), m_embeded));
	}

	return res;
}

elliptics::async_read_result_t::outer_result_t elliptics::async_read_result_t::get_one() {
	auto entry = m_inner_result.get_one();

	bool embeded = m_embeded;
	if (entry.io_attribute()->user_flags & UF_EMBEDS) {
		embeded = true;
	}

	return data_container_t::unpack(entry.file(), embeded);
}

void elliptics::async_read_result_t::wait()
{
	m_inner_result.wait();
}

elliptics::async_write_result_t::async_write_result_t(elliptics::async_write_result_t::inner_result_t &&inner_result, bool eblob_style_path, int base_port)
	: m_inner_result(std::move(inner_result))
	, m_eblob_style_path(eblob_style_path)
	, m_base_port(base_port)
{
}

elliptics::async_write_result_t::async_write_result_t(elliptics::async_write_result_t &&ob)
	: m_inner_result(std::move(ob.m_inner_result))
	, m_eblob_style_path(ob.m_eblob_style_path)
	, m_base_port(ob.m_base_port)
{
}

std::vector<elliptics::async_write_result_t::outer_result_t> elliptics::async_write_result_t::get()
{
	auto v = m_inner_result.get();
	std::vector<outer_result_t> res;

	for (auto it = v.begin(); it != v.end(); ++it) {
		res.push_back(lookup_result_t(*it, m_eblob_style_path, m_base_port));
	}

	return res;
}

elliptics::async_write_result_t::outer_result_t elliptics::async_write_result_t::get_one()
{
	auto l = m_inner_result.get_one();
	return lookup_result_t(l, m_eblob_style_path, m_base_port);
}

void elliptics::async_write_result_t::wait()
{
	m_inner_result.wait();
}

