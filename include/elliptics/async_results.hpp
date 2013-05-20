#ifndef _ELLIPTICS_ASYNC_RESULTS_HPP_
#define _ELLIPTICS_ASYNC_RESULTS_HPP_

#include "elliptics/result_entry.hpp"
#include "elliptics/data_container.hpp"
#include "elliptics/lookup_result.hpp"

namespace elliptics {

struct async_read_result_t {
	typedef ioremap::elliptics::read_result_entry inner_result_entry_t;
	typedef ioremap::elliptics::async_result<inner_result_entry_t> inner_result_t;
	typedef data_container_t outer_result_t;

	async_read_result_t(inner_result_t &&inner_result, bool embeded);

	async_read_result_t(async_read_result_t &&ob);

	std::vector<outer_result_t> get();
	outer_result_t get_one();
	void wait();

private:
	inner_result_t m_inner_result;
	bool m_embeded;
};

struct async_write_result_t {
	typedef ioremap::elliptics::async_write_result inner_result_t;
	typedef elliptics::lookup_result_t outer_result_t;

	async_write_result_t(inner_result_t &&inner_result, bool eblob_style_path, int base_port);

	async_write_result_t(async_write_result_t &&ob);

	std::vector<outer_result_t> get();
	outer_result_t get_one();
	void wait();

private:
	inner_result_t m_inner_result;
	bool m_eblob_style_path;
	int m_base_port;
};

typedef ioremap::elliptics::async_remove_result async_remove_result_t;

} // namespace elliptics

#endif /* _ELLIPTICS_ASYNC_RESULTS_HPP_ */
