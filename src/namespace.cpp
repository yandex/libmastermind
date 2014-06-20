#include "namespace_p.hpp"

namespace mastermind {

namespace_settings_t::data::data() {
}

namespace_settings_t::data::data(const namespace_settings_t::data &d)
	: name(d.name)
	, groups_count(d.groups_count)
	, success_copies_num(d.success_copies_num)
	, auth_key(d.auth_key)
	, static_couple(d.static_couple)
	, auth_key_for_write(d.auth_key_for_write)
	, auth_key_for_read(d.auth_key_for_read)
	, sign_token(d.sign_token)
	, sign_path_prefix(d.sign_path_prefix)
	, sign_port(d.sign_port)
{
}

namespace_settings_t::data::data(namespace_settings_t::data &&d)
	: name(std::move(d.name))
	, groups_count(std::move(d.groups_count))
	, success_copies_num(std::move(d.success_copies_num))
	, auth_key(std::move(d.auth_key))
	, static_couple(std::move(d.static_couple))
	, auth_key_for_write(std::move(d.auth_key_for_write))
	, auth_key_for_read(std::move(d.auth_key_for_read))
	, sign_token(std::move(d.sign_token))
	, sign_path_prefix(std::move(d.sign_path_prefix))
	, sign_port(std::move(d.sign_port))
{
}

namespace_settings_t::namespace_settings_t() {
}

namespace_settings_t::namespace_settings_t(const namespace_settings_t &ns)
	: m_data(new data(*ns.m_data))
{
}
namespace_settings_t::namespace_settings_t(namespace_settings_t &&ns)
	: m_data(std::move(ns.m_data))
{
}

namespace_settings_t::namespace_settings_t(data &&d)
	: m_data(new data(std::move(d)))
{
}

namespace_settings_t &namespace_settings_t::operator =(namespace_settings_t &&ns) {
	m_data = std::move(ns.m_data);
	return *this;
}

namespace_settings_t::~namespace_settings_t() {
}

const std::string &namespace_settings_t::name() const {
	return m_data->name;
}

int namespace_settings_t::groups_count() const {
	return m_data->groups_count;
}

const std::string &namespace_settings_t::success_copies_num () const {
	return m_data->success_copies_num;
}

const std::string &namespace_settings_t::auth_key () const {
	return m_data->auth_key.empty() ? m_data->auth_key_for_write : m_data->auth_key;
}

const std::vector<int> &namespace_settings_t::static_couple () const {
	return m_data->static_couple;
}

const std::string &namespace_settings_t::sign_token () const {
	return m_data->sign_token;
}

const std::string &namespace_settings_t::sign_path_prefix () const {
	return m_data->sign_path_prefix;
}

const std::string &namespace_settings_t::sign_port () const {
	return m_data->sign_port;
}

const std::string &namespace_settings_t::auth_key_for_write() const {
	return m_data->auth_key_for_write.empty() ? m_data->auth_key : m_data->auth_key_for_write;
}

const std::string &namespace_settings_t::auth_key_for_read() const {
	return m_data->auth_key_for_read;
}

} //mastermind

