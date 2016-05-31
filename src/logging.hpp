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

#ifndef SRC__LOGGING__HPP
#define SRC__LOGGING__HPP

#include <blackhole/logger.hpp>
#include <blackhole/attribute.hpp>
#include <blackhole/extensions/facade.hpp>

namespace mastermind { namespace logging {

enum severity: int {
    debug   =  0,
    info    =  1,
    warning =  2,
    error   =  3
};

}} // namespace mastermind::logging

// Assume __log__ to be std::shared_ptr<blackhole::logger_t>
#define MM_LOG(__log__, __severity__, ...) \
    blackhole::logger_facade<blackhole::logger_t>((*__log__.get())).log((__severity__), __VA_ARGS__)

#define MM_LOG_DEBUG(__log__, ...) \
    MM_LOG(__log__, ::mastermind::logging::debug, __VA_ARGS__)

#define MM_LOG_INFO(__log__, ...) \
    MM_LOG(__log__, ::mastermind::logging::info, __VA_ARGS__)

#define MM_LOG_WARNING(__log__, ...) \
    MM_LOG(__log__, ::mastermind::logging::warning, __VA_ARGS__)

#define MM_LOG_ERROR(__log__, ...) \
    MM_LOG(__log__, ::mastermind::logging::error, __VA_ARGS__)

#endif /* SRC__LOGGING__HPP */
