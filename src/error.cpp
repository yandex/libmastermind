#include "libmastermind/error.hpp"

namespace elliptics {

const char *libmastermind_category_impl::name() const {
	return "libmastermind";
}

std::string libmastermind_category_impl::message(int ev) const {
	switch (ev) {
	case libmastermind_error::couple_not_found:
		return "Couple not found";
	case libmastermind_error::not_enough_memory:
		return "There is no couple with enough memory";
	case libmastermind_error::unknown_namespace:
		return "Unknown namespace";
	case libmastermind_error::invalid_groups_count:
		return "Cannot find couple with such count of groups";
	default:
		return "Unknown libmastermind error";
	}
}

const std::error_category &libmastermind_category() {
	const static libmastermind_category_impl instance;
	return instance;
}

std::error_code make_error_code(libmastermind_error::libmastermind_error_t e) {
	return std::error_code(static_cast<int>(e), libmastermind_category());
}

std::error_condition make_error_condition(libmastermind_error::libmastermind_error_t e) {
	return std::error_condition(static_cast<int>(e), libmastermind_category());
}

couple_not_found_error::couple_not_found_error()
	: std::system_error(make_error_code(libmastermind_error::couple_not_found))
{}

not_enough_memory_error::not_enough_memory_error()
	: std::system_error(make_error_code(libmastermind_error::not_enough_memory))
{}

unknown_namespace_error::unknown_namespace_error()
	: std::system_error(make_error_code(libmastermind_error::unknown_namespace))
{}

invalid_groups_count_error::invalid_groups_count_error()
	: std::system_error(make_error_code(libmastermind_error::invalid_groups_count))
{}

} // namespace elliptics

