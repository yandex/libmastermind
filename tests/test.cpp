#include <iostream>

#include <elliptics/proxy.hpp>

using namespace elliptics;

int main(int argc, char* argv[])
{
	EllipticsProxy::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 8;
	//c.log_mask = 31;
	c.cocaine_config = std::string("/home/toshik/cocaine/cocaine_config.json");

	c.remotes.push_back(EllipticsProxy::remote("elisto19f.dev.yandex.net", 1025));

	EllipticsProxy proxy(c);

	sleep(1);
	Key k(std::string("test"));

	std::string data("test3");

	std::vector<int> lg = proxy.get_metabalancer_groups(3);

	std::cout << "Got groups: " << std::endl;
	for (std::vector<int>::const_iterator it = lg.begin(); it != lg.end(); it++)
		std::cout << *it << " ";
	std::cout << std::endl;

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

