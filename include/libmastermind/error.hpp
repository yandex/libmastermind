#ifndef INCLUDE__LIBMASTERMIND__ERROR_H
#define INCLUDE__LIBMASTERMIND__ERROR_H

#include <system_error>

namespace mastermind {

namespace libmastermind_error {

enum libmastermind_error_t {
	couple_not_found,
	not_enough_memory,
	unknown_namespace,
	invalid_groups_count
};

} // namespace libmastermind_error

class libmastermind_category_impl
	: public std::error_category
{
public:
	const char *name() const;
	std::string message(int ev) const;
};

const std::error_category &libmastermind_category();

std::error_code make_error_code(libmastermind_error::libmastermind_error_t e);
std::error_condition make_error_condition(libmastermind_error::libmastermind_error_t e);

class couple_not_found_error
	: public std::system_error
{
public:
	couple_not_found_error();
};

class not_enough_memory_error
	: public std::system_error
{
public:
	not_enough_memory_error();
};

class unknown_namespace_error
	: public std::system_error
{
public:
	unknown_namespace_error();
};

class invalid_groups_count_error
	: public std::system_error
{
public:
	invalid_groups_count_error();
};

} // namespace mastermind

namespace std {

template<>
struct is_error_code_enum<mastermind::libmastermind_error::libmastermind_error_t>
	: public true_type
{};

} // namespace std

#endif /* INCLUDE__LIBMASTERMIND__ERROR_H */
