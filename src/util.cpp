// libelliptics-proxy - smart proxy for Elliptics file storage
// Copyright (C) 2012 Anton Kortunov <toshik@yandex-team.ru>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include <sstream>
#include <cstring>

#include <fcntl.h>
#include <errno.h>
#include <netdb.h>

#include <sys/socket.h>

#include <elliptics/proxy.hpp>

#include "utils.hpp"

namespace elliptics {

elliptics_proxy_t::remote::remote(const std::string &host, const int port, const int family) :
				host(host), port(port), family(family)
{ }

elliptics_proxy_t::config::config() :
				log_path("/dev/stderr"),
				log_mask(DNET_LOG_INFO|DNET_LOG_ERROR),
				flags(0),
				wait_timeout(0),
				check_timeout(0),
				base_port(1024),
				directory_bit_num(32),
				success_copies_num(2),
				state_num(0),
				replication_count(0),
				chunk_size(0),
				eblob_style_path(true)
#ifdef HAVE_METABASE
				,group_weights_refresh_period(60)
#endif
{ }

bool dnet_id_less::operator () (const struct dnet_id &ob1, const struct dnet_id &ob2) {
	int res = memcmp(ob1.id, ob2.id, DNET_ID_SIZE);
	if (res == 0)
		res = ob1.type - ob2.type;
	return (res < 0);
}

}
