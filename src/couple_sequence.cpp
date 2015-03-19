/*
	Client library for mastermind
	Copyright (C) 2013-2015 Yandex

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

#include "libmastermind/couple_sequence.hpp"
#include "couple_sequence_p.hpp"

mastermind::couple_sequence_const_iterator_t::reference
mastermind::couple_sequence_const_iterator_t::operator * () const {
	return data->couples_info[data->current_index];
}

mastermind::couple_sequence_const_iterator_t::pointer
mastermind::couple_sequence_const_iterator_t::operator -> () const {
	return &data->couples_info[data->current_index];
}

bool
mastermind::couple_sequence_const_iterator_t::operator == (const self_type &other) const {
	if (data && other.data) {
		return data->current_index == other.data->current_index;
	}

	if (!data && !other.data) {
		return true;
	}

	auto d = (data ? data : other.data);

	if (d->current_index == d->couples_info.size()
			&& d->weighted_couples_info.empty()) {
		return true;
	}

	return false;
}

bool
mastermind::couple_sequence_const_iterator_t::operator != (const self_type &other) const {
	return !(*this == other);
}

mastermind::couple_sequence_const_iterator_t::self_type &
mastermind::couple_sequence_const_iterator_t::operator ++ () {
	data->try_extract_next();
	data->current_index += 1;
	return *this;
}

mastermind::couple_sequence_const_iterator_t::self_type
mastermind::couple_sequence_const_iterator_t::operator ++ (int) {
	self_type result;
	result.data = std::make_shared<data_t>(*data);
	++result;
	return result;
}

mastermind::couple_sequence_t::const_iterator
mastermind::couple_sequence_t::begin() const {
	if (!data) {
		return end();
	}

	auto d = std::make_shared<couple_sequence_const_iterator_init_t::data_t>(
			data->weighted_couples_info);
	return couple_sequence_const_iterator_init_t(std::move(d));
}

mastermind::couple_sequence_t::const_iterator
mastermind::couple_sequence_t::end() const {
	return couple_sequence_const_iterator_init_t(nullptr);
}

size_t
mastermind::couple_sequence_t::size() const {
	if (!data) {
		return 0;
	}

	return data->weighted_couples_info.size();
}

