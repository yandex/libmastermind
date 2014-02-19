#include <map>
#include <vector>
#include <sstream>

#include <boost/thread.hpp>

#include "utils.hpp"

namespace elliptics {

spent_time_printer_t::spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger)
: m_handler_name(handler_name)
, m_logger(logger)
, m_beg_time(std::chrono::system_clock::now())
{
	COCAINE_LOG_DEBUG(m_logger, "libmastermind: handling \'%s\'", m_handler_name.c_str());
}

spent_time_printer_t::~spent_time_printer_t() {
	auto end_time = std::chrono::system_clock::now();
	COCAINE_LOG_DEBUG(m_logger, "libmastermind: time spent for \'%s\': %d milliseconds"
		, m_handler_name.c_str()
		, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_beg_time).count())
		);
}

metabalancer_groups_info_t::metabalancer_groups_info_t(namespaces_t &&namespaces) {
	m_namespaces = std::move(namespaces);
	couples_by_namespaces_t cbn_tmp;

	for (auto nit = m_namespaces.begin(); nit != m_namespaces.end(); ++nit) {
		couples_by_groups_count_t cbgc_tmp;

		for (auto cit = nit->second.begin(); cit != nit->second.end(); ++cit) {
			couples_by_avalible_memory_t cbam_tmp;
			auto &couples_with_info = cit->second;
			std::sort(couples_with_info.begin(), couples_with_info.end(),
				couples_with_info_comp);

			for (size_t index = 0; index != couples_with_info.size(); ++index) {
				auto avalible_memory = std::get<2>(couples_with_info[index]);
				uint64_t total_weight = 0;

				for (size_t index2 = 0; index2 <= index; ++index2) {
					auto &couple_with_info = couples_with_info[index2];

					auto weight = std::get<1>(couple_with_info);
					if (weight == 0) {
						continue;
					}
					total_weight += weight;
					cbam_tmp[avalible_memory].insert(
						std::make_pair(total_weight, std::cref(std::get<0>(couple_with_info)))
					);
				}
			}

			cbgc_tmp.insert(std::make_pair(cit->first, cbam_tmp));
		}

		cbn_tmp.insert(std::make_pair(nit->first, cbgc_tmp));
	}

	m_couples.swap(cbn_tmp);

}

std::vector<int> metabalancer_groups_info_t::get_couple(uint64_t count, const std::string &name, uint64_t size) {
	auto nit = m_couples.find(name);
	if (nit == m_couples.end()) {
		throw std::runtime_error("Unknown namespace");
	}

	auto &groups_in_couple = nit->second;
	auto gicit = groups_in_couple.find(count);
	if (gicit == groups_in_couple.end()) {
		throw std::runtime_error("Unavalible count of groups in couple");
	}

	auto &avalible_memory = gicit->second;
	auto amit = avalible_memory.lower_bound(size);
	if (amit == avalible_memory.end()) {
		throw std::runtime_error("Not enought memory");
	}

	auto &weighted_groups = amit->second;
	if (weighted_groups.empty()) {
		throw std::runtime_error("Couple not found");
	}

	auto total_weight = weighted_groups.rbegin()->first;
	double shoot_point = double(random()) / RAND_MAX * total_weight;
	auto it = weighted_groups.lower_bound(uint64_t(shoot_point));

	if (it == weighted_groups.end()) {
		throw std::runtime_error("Couple not found");
	}

	return it->second;
}

bool metabalancer_groups_info_t::empty() {
	return m_couples.empty();
}

std::string metabalancer_groups_info_t::to_string() {
	std::ostringstream oss;

	oss << "{" << std::endl;

	for (auto beg_nit = m_namespaces.begin(), nit = beg_nit; nit != m_namespaces.end(); ++nit) {
		if (nit != beg_nit) oss << "," << std::endl;
		oss << "\t\"" << nit->first << "\" : {" << std::endl;

		for (auto beg_cit = nit->second.begin(), cit = beg_cit; cit != nit->second.end(); ++cit) {
			if (cit != beg_cit) oss << "," << std::endl;
			oss << "\t\t\"" << cit->first << "\" : {" << std::endl;

			for (auto beg_cwiit = cit->second.begin(), cwiit = beg_cwiit; cwiit != cit->second.end(); ++cwiit) {
				if (cwiit != beg_cwiit) oss << "," << std::endl;
				oss << "\t\t\t\"[";

				{
					auto &couple = std::get<0>(*cwiit);
					for (auto bit = couple.begin(), it = bit; it != couple.end(); ++it) {
						if (bit != it) oss << ", ";
						oss << *it;
					}
				}

				oss << "]\" : {" << std::endl;

				oss << "\t\t\t\t\"weight\" : " << std::get<1>(*cwiit) << "," << std::endl;
				oss << "\t\t\t\t\"memory\" : " << std::get<2>(*cwiit) << std::endl;

				oss << "\t\t\t}";
			}

			oss << std::endl << "\t\t}";
		}

		oss << std::endl << "\t}";
	}

	oss << std::endl << "}" << std::endl;

	return oss.str();
}

const metabalancer_groups_info_t::namespaces_t &metabalancer_groups_info_t::data() const {
	return m_namespaces;
}

bool metabalancer_groups_info_t::couples_with_info_comp(const couple_with_info_t &c1, const couple_with_info_t &c2) {
	return std::get<2>(c1) < std::get<2>(c2);
}

} // namespace elliptics

namespace msgpack {
elliptics::group_info_response_t &operator >> (object o, elliptics::group_info_response_t &v) {
	if (o.type != type::MAP) {
		throw type_error();
	}

	msgpack::object_kv *p = o.via.map.ptr;
	msgpack::object_kv *const pend = o.via.map.ptr + o.via.map.size;

	for (; p < pend; ++p) {
		std::string key;

		p->key.convert(&key);

		//			if (!key.compare("nodes")) {
		//				p->val.convert(&(v.nodes));
		//			}
		if (!key.compare("couples")) {
			p->val.convert(&(v.couples));
		}
		else if (!key.compare("status")) {
			std::string status;
			p->val.convert(&status);
			if (!status.compare("bad")) {
				v.status = elliptics::GROUP_INFO_STATUS_BAD;
			} else if (!status.compare("coupled")) {
				v.status = elliptics::GROUP_INFO_STATUS_COUPLED;
			}
		} else if (!key.compare("namespace")) {
			p->val.convert(&v.name_space);
		}
	}

	return v;
}
} // namespace msgpack
