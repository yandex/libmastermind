/*
	Client library for mastermind
	Copyright (C) 2013-2014 Yandex

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "namespace_p.hpp"
#include "utils.hpp"
#include "libmastermind/error.hpp"

#include <cocaine/traits/tuple.hpp>

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <map>
#include <vector>
#include <sstream>

namespace mastermind {

spent_time_printer_t::spent_time_printer_t(const std::string &handler_name, std::shared_ptr<cocaine::framework::logger_t> &logger)
: m_handler_name(handler_name)
, m_logger(logger)
, m_beg_time(std::chrono::system_clock::now())
{
	COCAINE_LOG_DEBUG(m_logger, "libmastermind: handling \'%s\'", m_handler_name.c_str());
}

spent_time_printer_t::~spent_time_printer_t() {
	auto end_time = std::chrono::system_clock::now();
	COCAINE_LOG_INFO(m_logger, "libmastermind: time spent for \'%s\': %d milliseconds"
		, m_handler_name.c_str()
		, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_beg_time).count())
		);
}

} // namespace mastermind

namespace msgpack {
mastermind::group_info_response_t &operator >> (object o, mastermind::group_info_response_t &v) {
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
				v.status = mastermind::GROUP_INFO_STATUS_BAD;
			} else if (!status.compare("coupled")) {
				v.status = mastermind::GROUP_INFO_STATUS_COUPLED;
			}
		} else if (!key.compare("namespace")) {
			p->val.convert(&v.name_space);
		}
	}

	return v;
}

std::vector<mastermind::namespace_settings_t> &operator >> (object o, std::vector<mastermind::namespace_settings_t> &v) {
	if (o.type != type::MAP) {
		throw type_error();
	}

	for (msgpack::object_kv *nit = o.via.map.ptr, *nit_end = nit + o.via.map.size; nit < nit_end; ++nit) {
		mastermind::namespace_settings_t::data item;

		nit->key.convert(&item.name);

		if (nit->val.type != type::MAP) {
			throw type_error();
		}

		for (msgpack::object_kv *it = nit->val.via.map.ptr, *it_end = it + nit->val.via.map.size; it < it_end; ++it) {
			std::string key;
			it->key.convert(&key);

			if (!key.compare("groups-count")) {
				it->val.convert(&item.groups_count);
			} else if (!key.compare("success-copies-num")) {
				it->val.convert(&item.success_copies_num);
			} else if (!key.compare("auth-key")) {
				it->val.convert(&item.auth_key);
			} else if (!key.compare("auth-keys")) {
				if (it->val.type != type::MAP) {
					throw type_error();
				}

				for (msgpack::object_kv *sit = it->val.via.map.ptr, *sit_end = sit + it->val.via.map.size;
						sit < sit_end; ++sit) {
					std::string key;
					sit->key.convert(&key);

					if (!key.compare("read")) {
						sit->val.convert(&item.auth_key_for_read);
					} else if (!key.compare("write")) {
						sit->val.convert(&item.auth_key_for_write);
					}
				}
			} else if (!key.compare("static-couple")) {
				it->val.convert(&item.static_couple);
			} else if (!key.compare("signature")) {
				if (it->val.type != type::MAP) {
					throw type_error();
				}

				for (msgpack::object_kv *sit = it->val.via.map.ptr, *sit_end = sit + it->val.via.map.size;
						sit < sit_end; ++sit) {
					std::string key;
					sit->key.convert(&key);

					if (!key.compare("token")) {
						sit->val.convert(&item.sign_token);
					} else if (!key.compare("path_prefix")) {
						sit->val.convert(&item.sign_path_prefix);
					} else if (!key.compare("port")) {
						int port;
						sit->val.convert(&port);
						item.sign_port = boost::lexical_cast<std::string>(port);
					}
				}
			} else if (!key.compare("redirect")) {
				if (it->val.type != type::MAP) {
					throw type_error();
				}

				for (msgpack::object_kv *rit = it->val.via.map.ptr, *rit_end = rit + it->val.via.map.size;
						rit < rit_end; ++rit) {
					std::string key;
					rit->key.convert(&key);

					if (!key.compare("content-length-threshold")) {
						rit->val.convert(&item.redirect_content_length_threshold);
					} else if (!key.compare("expire-time")) {
						rit->val.convert(&item.redirect_expire_time);
					}
				}
			} else if (!key.compare("is_active")) {
				it->val.convert(&item.is_active);
			} else if (!key.compare("features")) {
				if (it->val.type != type::MAP) {
					throw type_error();
				}

				for (msgpack::object_kv *fit = it->val.via.map.ptr, *fit_end = fit + it->val.via.map.size;
						fit < fit_end; ++fit) {
					std::string key;
					fit->key.convert(&key);

					if (!key.compare("select-couple-to-upload")) {
						fit->val.convert(&item.can_choose_couple_to_upload);
					} else if (!key.compare("multipart")) {
						if (fit->val.type != type::MAP) {
							throw type_error();
						}

						for (msgpack::object_kv *mit = fit->val.via.map.ptr, *mit_end = mit + fit->val.via.map.size;
								mit < mit_end; ++mit) {
							std::string key;
							mit->key.convert(&key);

							if (!key.compare("content-length-threshold")) {
								mit->val.convert(&item.multipart_content_length_threshold);
							}
						}
					}
				}
			}
		}

		v.emplace_back(std::move(item));
	}

	return v;
}

packer<sbuffer> &operator << (packer<sbuffer> &o, const std::vector<mastermind::namespace_settings_t> &v) {
	o.pack_map(v.size());

	for (auto it = v.begin(); it != v.end(); ++it) {
		o.pack(it->name());

		o.pack_map(9);

		o.pack(std::string("groups-count"));
		o.pack(it->groups_count());

		o.pack(std::string("success-copies-num"));
		o.pack(it->success_copies_num());

		o.pack(std::string("auth-key"));
		o.pack(it->auth_key());

		o.pack(std::string("auth-keys"));
		o.pack_map(2);

		o.pack(std::string("write"));
		o.pack(it->auth_key_for_write());

		o.pack(std::string("read"));
		o.pack(it->auth_key_for_read());

		o.pack(std::string("static-couple"));
		o.pack(it->static_couple());

		o.pack(std::string("signature"));

		const auto &sp = it->sign_port();
		if (!sp.empty()) {
			o.pack_map(3);
		} else {
			o.pack_map(2);
		}

		o.pack(std::string("token"));
		o.pack(it->sign_token());

		o.pack(std::string("path_prefix"));
		o.pack(it->sign_path_prefix());

		if (!sp.empty()) {
			o.pack(std::string("port"));
			o.pack(boost::lexical_cast<int>(sp));
		}

		o.pack(std::string("redirect"));
		o.pack_map(2);

		o.pack(std::string("content-length-threshold"));
		o.pack(it->redirect_content_length_threshold());

		o.pack(std::string("expire-time"));
		o.pack(it->redirect_expire_time());

		o.pack(std::string("is_active"));
		o.pack(it->is_active());

		o.pack(std::string("features"));
		o.pack_map(2);

		o.pack(std::string("select-couple-to-upload"));
		o.pack(it->can_choose_couple_to_upload());

		o.pack(std::string("multipart"));
		o.pack_map(1);

		o.pack(std::string("content-length-threshold"));
		o.pack(it->multipart_content_length_threshold());
	}

	return o;
}

mastermind::metabalancer_info_t &operator >> (object o, mastermind::metabalancer_info_t &r) {
	if (o.type != type::ARRAY) {
		throw type_error();
	}

	for (auto array_ptr = o.via.array.ptr, array_end = array_ptr + o.via.array.size;
			array_ptr != array_end; ++array_ptr)
	{
		if (array_ptr->type != type::MAP) {
			throw type_error();
		}

		auto couple_info = std::make_shared<mastermind::couple_info_t>();

		for (auto couple_kv = array_ptr->via.map.ptr,
				couple_kv_end = couple_kv + array_ptr->via.map.size;
				couple_kv_end != couple_kv;
				++couple_kv)
		{
			std::string key;
			couple_kv->key.convert(&key);

			if (key == "id") {
				couple_kv->val.convert(&couple_info->id);
			} else if (key == "namespace") {
				couple_kv->val.convert(&couple_info->ns);
			} else if (key == "tuple") {
				couple_kv->val.convert(&couple_info->tuple);
			} else if (key == "used_space") {
				couple_kv->val.convert(&couple_info->used_space);
			} else if (key == "free_space") {
				couple_kv->val.convert(&couple_info->free_space);
			} else if (key == "free_effective_space") {
				couple_kv->val.convert(&couple_info->free_effective_space);
			} else if (key == "couple_status") {
				std::string couple_status;
				couple_kv->val.convert(&couple_status);
				if (couple_status == "OK") {
					couple_info->couple_status = mastermind::couple_info_t::OK;
				} else if (couple_status == "BAD") {
					couple_info->couple_status = mastermind::couple_info_t::BAD;
				} else {
					couple_info->couple_status = mastermind::couple_info_t::UNKNOWN;
				}
			} else if (key == "groups") {
				if (couple_kv->val.type != type::ARRAY) {
					throw type_error();
				}

				auto group_info = std::make_shared<mastermind::group_info_t>();

				for (auto group_array = couple_kv->val.via.array.ptr,
						group_array_end = group_array + couple_kv->val.via.array.size;
						group_array != group_array_end;
						++group_array)
				{
					if (group_array->type != type::MAP) {
						throw type_error();
					}

					for (auto group_kv = group_array->via.map.ptr,
							group_kv_end = group_kv + group_array->via.map.size;
							group_kv != group_kv_end;
							++group_kv)
					{
						std::string key;
						group_kv->key.convert(&key);

						if (key == "id") {
							group_kv->val.convert(&group_info->id);
						} else if (key == "namespace") {
							group_kv->val.convert(&group_info->ns);
						} else if (key == "status") {
							std::string status;
							group_kv->val.convert(&status);

							if (status == "COUPLED") {
								group_info->group_status = mastermind::group_info_t::COUPLED;
							} else if (status == "BAD") {
								group_info->group_status = mastermind::group_info_t::BAD;
							} else {
								group_info->group_status = mastermind::group_info_t::UNKNOWN;
							}
						} else if (key == "couples"){
							group_kv->val.convert(&group_info->couple);
						}
					}
					group_info->couple_info = mastermind::couple_info_weak_ptr_t(couple_info);
					auto insert_res = r.group_info_map.insert(std::make_pair(group_info->id, group_info));
					if (insert_res.second) {
						couple_info->group_info.emplace_back(group_info);
					}
				}

			}
		}

		r.couple_info_map.insert(std::make_pair(couple_info->id, couple_info));
		r.namespace_info_map[couple_info->ns].emplace_back(couple_info);
	}

	return r;
}

packer<sbuffer> &operator << (packer<sbuffer> &o, const mastermind::metabalancer_info_t &r) {
	o.pack_array(r.couple_info_map.size());

	for (auto cit = r.couple_info_map.begin(), cit_end = r.couple_info_map.end(); cit != cit_end; ++cit) {
		o.pack_map(8);

		o.pack(std::string("id"));
		o.pack(cit->second->id);

		o.pack(std::string("namespace"));
		o.pack(cit->second->ns);

		o.pack(std::string("tuple"));
		o.pack(cit->second->tuple);

		o.pack(std::string("used_space"));
		o.pack(cit->second->used_space);

		o.pack(std::string("free_space"));
		o.pack(cit->second->free_space);

		o.pack(std::string("free_effective_space"));
		o.pack(cit->second->free_effective_space);

		o.pack(std::string("couple_status"));
		switch (cit->second->couple_status) {
		case mastermind::couple_info_t::OK:
			o.pack(std::string("OK"));
			break;
		default:
			o.pack(std::string("UNKNOWN"));
		}

		o.pack(std::string("groups"));
		o.pack_array(cit->second->tuple.size());

		for (auto git = cit->second->tuple.begin(), git_end = cit->second->tuple.end();
				git != git_end; ++git)
		{
			auto it = r.group_info_map.find(*git);
			if (it == r.group_info_map.end()) {
				continue;
			}

			o.pack_map(4);

			o.pack(std::string("id"));
			o.pack(it->second->id);

			o.pack(std::string("namespace"));
			o.pack(it->second->ns);

			o.pack(std::string("status"));
			switch (it->second->group_status) {
			case mastermind::group_info_t::COUPLED:
				o.pack(std::string("COUPLED"));
				break;
			case mastermind::group_info_t::BAD:
				o.pack(std::string("BAD"));
				break;
			default:
				o.pack(std::string("UNKNOWN"));
			}

			o.pack(std::string("couples"));
			o.pack(it->second->couple);
		}
	}
	return o;
}

} // namespace msgpack
