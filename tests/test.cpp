#include <iostream>

#include <elliptics/proxy.hpp>

using namespace elliptics;

int main(int argc, char* argv[])
{
	EllipticsProxy::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	c.cocaine_config = std::string("/home/toshik/cocaine/cocaine_config.json");

	c.remotes.push_back(EllipticsProxy::remote("elisto22f.dev.yandex.net", 1025));

	EllipticsProxy proxy(c);

	sleep(1);
	Key k(std::string("test"));

	std::string data("test3");

	/*
 	std::vector<int> lg = proxy.get_metabalancer_groups(3);

	std::cout << "Got groups: " << std::endl;
	for (std::vector<int>::const_iterator it = lg.begin(); it != lg.end(); it++)
		std::cout << *it << " ";
	std::cout << std::endl;
	*/

	GroupInfoResponse gi = proxy.get_metabalancer_group_info(103);
	std::cout << "Got info from mastermind: status: " << gi.status << ", " << gi.nodes.size() << " groups: ";
	for (std::vector<int>::const_iterator it = gi.couples.begin(); it != gi.couples.end(); it++)
		std::cout << *it << " ";
	std::cout << std::endl;

	std::vector<EllipticsProxy::remote> remotes = proxy.lookup_addr(k, gi.couples);
	for (std::vector<EllipticsProxy::remote>::const_iterator it = remotes.begin(); it != remotes.end(); ++it)
		std::cout << it->host << " ";
	std::cout << std::endl;

        struct dnet_id id;

        memset(&id, 0, sizeof(id));
        for (int i = 0; i < DNET_ID_SIZE; i++)
            id.id[i] = i;

        ID id1(id);
        Key key1(id1);

        memset(&id, 0, sizeof(id));
        for (int i = 0; i < DNET_ID_SIZE; i++)
            id.id[i] = i+16;

        ID id2(id);
        Key key2(id2);

        std::cout << "ID1: " << id1.str() << " " << (id1 < id2) << " ID2: " << id2.str() << std::endl;
        std::cout << "Key1: " << key1.str() << " " << (key1 < key2) << " Key2: " << key2.str() << std::endl;



        

	return 0;

	std::vector<LookupResult>::const_iterator it;
	std::vector<LookupResult> l = proxy.write(k, data);
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (it = l.begin(); it != l.end(); ++it) {
		std::cout << "		path: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	for (int i = 0; i < 20; i++) {
		LookupResult l1 = proxy.lookup(k);
		std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
	}

	ReadResult res = proxy.read(k);

	std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!" << res.data << std::endl;

	return 0;
}

