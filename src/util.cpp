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

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/scoped_array.hpp>

//#include <eblob/blob.h>

#include <elliptics/proxy.hpp>

namespace elliptics {

EllipticsProxy::remote::remote(const std::string &host, const int port, const int family) :
				host(host), port(port), family(family)
{ }

EllipticsProxy::config::config() :
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
{ }

ID::ID() : empty_(true)
{
	std::memset(&id_, 0, sizeof(id_));
}

ID::ID(struct dnet_id &id) :
	id_(id),
	empty_(false)
{ }

bool ID::operator==(const ID &id2) const
{
        int res = memcmp(id_.id, id2.id_.id, DNET_ID_SIZE);

        return (res == 0) && id_.type == id2.id_.type;
}

bool ID::operator<(const ID &id2) const
{
        int res = memcmp(id_.id, id2.id_.id, DNET_ID_SIZE);

        return (res < 0) && id_.type < id2.id_.type;
}

std::string ID::str() const
{
	return dump();
}

std::string ID::dump(unsigned int len) const
{
	std::string res(dnet_dump_id_len(&id_, len));
	return res;
}

struct dnet_id ID::dnet_id() const {
	return id_;
}

Key::Key() :
	byId_(true),
	id_(ID())
{ }

Key::Key(std::string filename, int column) :
	byId_(false),
	filename_(filename),
	column_(column)
{ }

Key::Key(ID &id) :
	byId_(true),
	id_(id)
{
	column_ = id_.dnet_id().type;
}

bool Key::operator==(const Key &key2) const
{
        if (!byId_ && !key2.byId_)
            return filename_ == key2.filename_;

        if (byId_ && key2.byId_)
            return id_ == key2.id_;

        return false;
}
bool Key::operator<(const Key &key2) const
{
// IDs are always less than filenames
        if (byId_) {
            if (key2.byId_) {
                return id_ < key2.id_;
            } else {
                return true;
            }
        } else {
            if (!key2.byId_) {
                return filename_ < key2.filename_;
            } else {
                return false;
            }
        }
}


bool Key::byId() const
{
	return byId_;
}

const std::string Key::filename() const
{
	if (!byId_) {
		return filename_;
	} else {
		return std::string();
	}
}

const int Key::column() const {
	return column_;
}

struct dnet_id Key::dnet_id() const {
	return id_.dnet_id();
}

const ID Key::id() const {
	return id_;
}

const std::string Key::str() const {
	if (byId_) {
		return id_.str();
	} else {
		return filename_;
	}
}

void Key::transform(ioremap::elliptics::session &sess)
{
        struct dnet_id id;
        memset(&id, 0, sizeof(id));

        sess.transform(filename_, id);

        id_ = ID(id);
}

}
