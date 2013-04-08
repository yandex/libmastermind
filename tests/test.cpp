#include <iostream>

#include <elliptics/proxy.hpp>

using namespace elliptics;

void test_lookup () {
	elliptics_proxy_t::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	c.remotes.push_back(elliptics_proxy_t::remote("derikon.dev.yandex.net", 1025, 2));
	c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;

	elliptics_proxy_t proxy(c);
	sleep(1);

	elliptics::key_t k(std::string("uniq_key"));

	proxy.remove (k);

	std::string data("test3");

	std::vector <int> g = {2};
	std::vector<lookup_result_t> l = proxy.write(k, data, _groups = g);
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	lookup_result_t l1 = proxy.lookup(k);
	std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
}

void test_read_async () {
	elliptics_proxy_t::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	c.remotes.push_back(elliptics_proxy_t::remote("derikon.dev.yandex.net", 1025, 2));
	c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;

	elliptics_proxy_t proxy(c);
	sleep(1);

	elliptics::key_t k(std::string("uniq_key"));

	proxy.remove (k);

	std::string data("test3");

	//std::vector <int> g = {2};
	std::vector<lookup_result_t> l = proxy.write(k, data/*, _groups = g*/);
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	{
		async_read_result_t arr = proxy.read_async(k);
		std::cout << "Wait result..." << std::endl;
		auto r = arr.get ();
		std::cout << "Read result: " << r.data << std::endl;
	}

	{
		async_read_result_t arr = proxy.read_async(k);
	}
	std::cout << "Forgot about waiter before getting result" << std::endl;

}

void test_async () {

	elliptics_proxy_t::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	c.remotes.push_back(elliptics_proxy_t::remote("derikon.dev.yandex.net", 1025, 2));
	c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;

	elliptics_proxy_t proxy(c);
	sleep(1);

	elliptics::key_t k1(std::string("111uniq_key1.txt"));
	elliptics::key_t k2(std::string("111uniq_key2.txt"));

	try { proxy.remove_async (k1).get (); } catch (...) {}
	try { proxy.remove_async (k2).get (); } catch (...) {}

	std::string data1("data1");
	std::string data2("data2");

	std::vector<lookup_result_t> l;

	auto awr1 = proxy.write_async(k1, data1);
	auto awr2 = proxy.write_async(k2, data2);

	try {
		l = awr1.get ();
	} catch (...) {
		std::cout << "Exception during get write result" << std::endl;
		return;
	}
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	try {
		l = awr2.get ();
	} catch (...) {
		std::cout << "Exception during get write2 result" << std::endl;
		return;
	}
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	async_read_result_t arr1 = proxy.read_async(k1);
	async_read_result_t arr2 = proxy.read_async(k2);

	read_result_t r;
	try {
		r = arr1.get ();
	} catch (...) {
		std::cout << "Exception during get read result" << std::endl;
		return;
	}
	std::cout << "Read result: " << r.data << std::endl;

	try {
		r = arr2.get ();
	} catch (...) {
		std::cout << "Exception during get read result" << std::endl;
		return;
	}
	std::cout << "Read result: " << r.data << std::endl;
}

void test_sync () {
	elliptics_proxy_t::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	c.remotes.push_back(elliptics_proxy_t::remote("derikon.dev.yandex.net", 1025, 2));
	c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;

	elliptics_proxy_t proxy(c);
	sleep(1);

	elliptics::key_t k1(std::string("111uniq_key1.txt"));

	try { proxy.remove (k1); } catch (...) {}

	std::string data1("data1");

	std::vector<lookup_result_t> l = proxy.write(k1, data1);

	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	std::cout << "Read result: " << proxy.read(k1).data << std::endl;
}

int main(int argc, char* argv[])
{
	//test_lookup ();
	//test_read_async ();
	test_async ();
	//test_sync ();
	return 0;
	elliptics_proxy_t::config c;
	c.groups.push_back(1);
	c.groups.push_back(2);
	c.log_mask = 1;
	//c.cocaine_config = std::string("/home/toshik/cocaine/cocaine_config.json");

	//c.remotes.push_back(EllipticsProxy::remote("elisto22f.dev.yandex.net", 1025));
	c.remotes.push_back(elliptics_proxy_t::remote("derikon.dev.yandex.net", 1025, 2));

	elliptics_proxy_t proxy(c);

	sleep(1);
	elliptics::key_t k(std::string("test"));

	std::string data("test3");

	std::vector <elliptics::key_t> keys = {std::string ("key5"), std::string ("key6")};
	std::vector <std::string> data_arr = {"data1", "data2"};
	//std::vector <Key> keys = {std::string ("key1")};
	//std::vector <std::string> data_arr = {"data1"};
	std::vector <int> groups = {1, 2};

	{
		auto results = proxy.bulk_write (keys, data_arr, _groups = groups, _replication_count = groups.size ());
		for (auto it = results.begin (), end = results.end (); it != end; ++it) {
			std::cout << it->first.to_string () << ':' << std::endl;
			for (auto it2 = it->second.begin (), end2 = it->second.end (); it2 != end2; ++it2) {
				std::cout << "\tgroup: " << it2->group << "\tpath: " << it2->hostname
									  << ":" << it2->port << it2->path << std::endl;
			}
		}
	}

	/*for (int i = 0; i != 2; ++i) {
		auto result = proxy.write (keys [i], data_arr [i], _groups = groups, _replication_count = groups.size ());
		std::cout << keys [i].str () << ':' << std::endl;
		for (auto it = result.begin (), end = result.end (); it != end; ++it) {
			std::cout << "\tgroup: " << it->group << "\tpath: " << it->hostname
								  << ":" << it->port << it->path << std::endl;
		}
	}*/

	/*std::vector <int> g = {2};

	for (int i = 0; i != 2; ++i){
		auto result = proxy.read (keys [0], _groups = std::vector <int> (1, i + 1));
		std::cout << result.data << std::endl;
	}*/

	{
		auto result = proxy.bulk_read (keys, _groups = groups);
		//std::cout << "Read data: " << result.data << std::endl;
		for (auto it = result.begin (), end = result.end (); it != end; ++it) {
			//std::cout << it->first.str () << '\t' << it->second.data << std::endl;
			std::cout << it->second.data << std::endl;
		}
	}

	return 0;

	/*
 	std::vector<int> lg = proxy.get_metabalancer_groups(3);

	std::cout << "Got groups: " << std::endl;
	for (std::vector<int>::const_iterator it = lg.begin(); it != lg.end(); it++)
		std::cout << *it << " ";
	std::cout << std::endl;
	*/

	group_info_response_t gi = proxy.get_metabalancer_group_info(103);
	std::cout << "Got info from mastermind: status: " << gi.status << ", " << gi.nodes.size() << " groups: ";
	for (std::vector<int>::const_iterator it = gi.couples.begin(); it != gi.couples.end(); it++)
		std::cout << *it << " ";
	std::cout << std::endl;

	std::vector<elliptics_proxy_t::remote> remotes = proxy.lookup_addr(k, gi.couples);
	for (std::vector<elliptics_proxy_t::remote>::const_iterator it = remotes.begin(); it != remotes.end(); ++it)
		std::cout << it->host << " ";
	std::cout << std::endl;

		struct dnet_id id;

		memset(&id, 0, sizeof(id));
		for (int i = 0; i < DNET_ID_SIZE; i++)
			id.id[i] = i;

		elliptics::key_t key1(id);

		memset(&id, 0, sizeof(id));
		for (int i = 0; i < DNET_ID_SIZE; i++)
			id.id[i] = i+16;

		elliptics::key_t key2(id);


		std::cout << "ID1: " << dnet_dump_id_len (&key1.id (), 6) << " "
				  << " ID2: " << dnet_dump_id_len (&key2.id (), 6) << std::endl;
		std::cout << "Key1: " << key1.to_string () << " " << (key1 < key2) << " Key2: " << key2.to_string () << std::endl;





	return 0;

	std::vector<lookup_result_t>::const_iterator it;
	std::vector<lookup_result_t> l = proxy.write(k, data);
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (it = l.begin(); it != l.end(); ++it) {
		std::cout << "		path: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	for (int i = 0; i < 20; i++) {
		lookup_result_t l1 = proxy.lookup(k);
		std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
	}

	read_result_t res = proxy.read(k);

	std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!" << res.data << std::endl;

	return 0;
}

