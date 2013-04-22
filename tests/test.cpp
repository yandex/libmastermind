#include <iostream>
#include <initializer_list>
#include <functional>
#include <cstdlib>
#include <sstream>

#include <elliptics/proxy.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <stdexcept>

#include <cppunit/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "teamcity_cppunit.h"


using namespace elliptics;

using namespace CppUnit;

class elliptics_tests_t : public TestFixture {
public:
	elliptics_tests_t(const std::string &script)
	: m_script(script) {
		std::system((script + " prepare 4").c_str());
		nodes<start>();

		elliptics_proxy_t::config elconf;
		elconf.groups.push_back(1);
		elconf.groups.push_back(2);
		elconf.log_mask = 1;
		//elconf.cocaine_config = std::string("/home/derikon/cocaine/cocaine_config.json");
		elconf.remotes.push_back(elliptics_proxy_t::remote("localhost", 1025, 2));
		elconf.success_copies_num = SUCCESS_COPIES_TYPE__ALL;

		m_proxy.reset(new elliptics_proxy_t(elconf));
		sleep(1);
	}

	~elliptics_tests_t() {
		m_proxy.reset();
		nodes<stop>();
		std::system((m_script + " clear").c_str());
	}

	void test_write_g4_scnALL() {
		nodes<start>();
		auto lrs = write(0, 0, 0, 0, {1, 2, 3, 4}, SUCCESS_COPIES_TYPE__ALL);
		CPPUNIT_ASSERT(lrs.size() == 4);
	}

	void test_write_g3_scnALL() {
		nodes<start>();
		auto lrs = write(0, 0, 0, 0, {1, 2, 3}, SUCCESS_COPIES_TYPE__ALL);
		CPPUNIT_ASSERT(lrs.size() == 3);
	}

	void test_write_g2_1_scnALL() {
		nodes<start>();
		node<stop>(3);
		std::vector<lookup_result_t> lrs;
		try { lrs = write(0, 0, 0, 0, {1, 2, 3}, SUCCESS_COPIES_TYPE__ALL); } catch (...) {}
		node<start>(3);
		CPPUNIT_ASSERT(lrs.size() == 0);
	}

	void test_write_g2_1_scnQUORUM() {
		nodes<start>();
		node<stop>(3);
		auto lrs = write(0, 0, 0, 0, {1, 2, 3}, SUCCESS_COPIES_TYPE__QUORUM);
		node<start>(3);
		CPPUNIT_ASSERT(lrs.size() == 2);
	}

	void test_write_g1_2_scnQUORUM() {
		nodes<start>();
		nodes<stop>({2,3});
		std::vector<lookup_result_t> lrs;
		try { lrs = write(0, 0, 0, 0, {1, 2, 3}, SUCCESS_COPIES_TYPE__QUORUM); } catch (...) {}
		nodes<start>({2,3});
		CPPUNIT_ASSERT(lrs.size() == 0);
	}

	void test_write_g1_2_scnANY() {
		nodes<start>();
		nodes<stop>({2,3});
		auto lrs = write(0, 0, 0, 0, {1, 2, 3}, SUCCESS_COPIES_TYPE__ANY);
		nodes<start>({2,3});
		CPPUNIT_ASSERT(lrs.size() == 1);
	}

	void test_lookup() {
		elliptics::key_t k(std::string("key.txt"));
		std::string data("data");

		std::vector <int> g = {2};
		std::vector<lookup_result_t> l = m_proxy->write(k, data/*, _groups = g*/);
		std::cout << "written " << l.size() << " copies" << std::endl;
		for (auto it = l.begin(); it != l.end(); ++it) {
			std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
		}

		auto l1 = m_proxy->lookup(k);
		std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
	}
private:
	std::vector<lookup_result_t> write(uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> groups, int success_copies_num) {
		//static size_t num = 0;
		//num += 1;
		//elliptics::key_t key(std::string("key_") + boost::lexical_cast<std::string>(num));
		elliptics::key_t key(std::string("key"));
		std::string data("data");

		try { m_proxy->remove(key); } catch (...) {}

		return m_proxy->write(key, data, _offset = offset, _size = size, _cflags = cflags, _ioflags = ioflags, _groups = groups, _success_copies_num = success_copies_num);
	}

	enum node_action { start, stop };

	template<node_action na>
	void node(size_t num) {
		std::ostringstream oss;
		oss << m_script;
		if(na == start) oss << " start ";
		else if (na == stop) oss << " stop ";
		oss << num;
		std::system(oss.str().c_str());
	}

	template<node_action na>
	void nodes(std::initializer_list<size_t> nums) {
		for(auto it = nums.begin(); it != nums.end(); ++it)
			node<na>(*it);
	}

	template<node_action na>
	void nodes() {
		std::ostringstream oss;
		oss << m_script;
		if(na == start) oss << " start ";
		else if (na == stop) oss << " stop ";
		std::system(oss.str().c_str());
	}

	std::shared_ptr<elliptics_proxy_t> m_proxy;
	std::string m_script;
};

void test_lookup (elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	elliptics::key_t k(std::string("key.txt"));

	try { proxy.remove (k); } catch (...) {}

	std::string data("data");

	std::vector <int> g = {2};
	std::vector<lookup_result_t> l = proxy.write(k, data/*, _groups = g*/);
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	auto l1 = proxy.lookup(k);
	std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
}

void test_async (elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	elliptics::key_t k1(std::string("key1.txt"));
	elliptics::key_t k2(std::string("key2.txt"));

	try { proxy.remove_async (k1).get (); } catch (...) {}
	try { proxy.remove_async (k2).get (); } catch (...) {}

	std::string data1("data1");
	std::string data2("data2");

	std::vector<ioremap::elliptics::lookup_result_entry> l;

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
		//std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
		auto host = proxy.get_host (*it);
		auto path = proxy.get_path (*it);
		std::cout << "\tpath: " << host.host << ":" << host.port << path << std::endl;
	}

	try {
		l = awr2.get ();
	} catch (...) {
		std::cout << "Exception during get write2 result" << std::endl;
		return;
	}
	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		//std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
		auto host = proxy.get_host (*it);
		auto path = proxy.get_path (*it);
		std::cout << "\tpath: " << host.host << ":" << host.port << path << std::endl;
	}

	auto arr1 = proxy.read_async(k1);
	auto arr2 = proxy.read_async(k2);

	data_container_t ds;
	try {
		ds = arr1.get();
	} catch (...) {
		std::cout << "Exception during get read result" << std::endl;
		return;
	}
	std::cout << "Read result: " << ds.data.to_string() << std::endl;

	try {
		ds = arr2.get();
	} catch (...) {
		std::cout << "Exception during get read result" << std::endl;
		return;
	}
	std::cout << "Read result: " << ds.data.to_string() << std::endl;
}

void test_sync (elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	elliptics::key_t k1(std::string("key1.txt"));

	try { proxy.remove (k1); } catch (...) {}

	std::string data1("data1");

	std::vector<lookup_result_t> l = proxy.write(k1, data1);

	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	std::cout << "Read result: " << proxy.read(k1).data.to_string() << std::endl;
}

void test_sync_embeds (elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	elliptics::key_t k1(std::string("key1.txt"));

	try { proxy.remove (k1); } catch (...) {}

	data_container_t ds("data1");
	timespec ts;
	ts.tv_sec = 123;
	ts.tv_nsec = 456000;
	ds.set<elliptics::DNET_FCGI_EMBED_TIMESTAMP>(ts);

	std::vector<lookup_result_t> l = proxy.write(k1, ds);

	std::cout << "written " << l.size() << " copies" << std::endl;
	for (auto it = l.begin(); it != l.end(); ++it) {
		std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
	}

	auto rr = proxy.read(k1, _embeded = true);
	std::cout << "Read result: " << rr.data.to_string() << std::endl;
	auto r0 = rr.get<elliptics::DNET_FCGI_EMBED_TIMESTAMP>();
	if (r0) {
		timespec &ts = *r0;
		std::cout << "Embed result: " << ts.tv_sec << ' ' << ts.tv_nsec << std::endl;
	}
	else
		std::cout << "Embed result: none" << std::endl;
}

void test_bulk_sync(elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	std::vector <elliptics::key_t> keys = {std::string ("key5"), std::string ("key6")};
	std::vector <data_container_t> data_arr;
	data_arr.emplace_back("data1");
	data_arr.emplace_back("data2");
	std::vector <int> groups = {1, 2};

	try { proxy.remove (keys[0]); } catch (...) {}
	try { proxy.remove (keys[1]); } catch (...) {}

	{
		auto results = proxy.bulk_write (keys, data_arr, _groups = groups);
		for (auto it = results.begin (), end = results.end (); it != end; ++it) {
			std::cout << it->first.to_string () << ':' << std::endl;
			for (auto it2 = it->second.begin (), end2 = it->second.end (); it2 != end2; ++it2) {
				std::cout << "\tgroup: " << it2->group << "\tpath: " << it2->hostname
									  << ":" << it2->port << it2->path << std::endl;
			}
		}
	}

	{
		auto result = proxy.bulk_read (keys, _groups = groups);
		for (auto it = result.begin (), end = result.end (); it != end; ++it) {
			std::cout << it->second.data.to_string() << std::endl;
		}
	}
}

void test_mastermind_groups(elliptics_proxy_t &proxy) {
	std::cout << __func__ << std::endl;

	auto r1 = proxy.get_symmetric_groups();
	std::cout << "get_symmetric_groups: " << std::endl;
	std::cout << "size = " << r1.size() << std::endl;
	for (size_t i = 0; i != r1.size(); ++i) {
		std::cout << "\tsize = " << r1[i].size() << std::endl;
		std::cout << "\t\t";
		for (size_t j = 0; j != r1[i].size(); ++j) {
			std::cout << r1[i][j] << ' ';
		}
		std::cout << std::endl;
	}

	auto r2 = proxy.get_bad_groups();
	std::cout << "get_bad_groups: " << std::endl;
	for (auto it = r2.begin(); it != r2.end(); ++it) {
		std::cout << it->first << std::endl;
		std::cout << "\tsize: " << it->second.size() << std::endl;
		std::cout << "\t\t";
		for (auto jt = it->second.begin(); jt != it->second.end(); ++jt) {
			std::cout << *jt << ' ';
		}
		std::cout << std::endl;
	}

	auto r3 = proxy.get_all_groups();
	std::cout << "get_all_groups: " << std::endl;
	for (auto it = r3.begin(); it != r3.end(); ++it) {
		std::cout << *it << ' ';
	}
	std::cout << std::endl;
}

class tester {
public:
	typedef std::function<void (elliptics_proxy_t &)> test_t;
	typedef std::initializer_list<test_t> tests_t;

	tester(const std::string &host, int port, int family) {
		elliptics_proxy_t::config elconf;
		elconf.groups.push_back(1);
		elconf.groups.push_back(2);
		elconf.log_mask = 1;
		elconf.cocaine_config = std::string("/home/derikon/cocaine/cocaine_config.json");
		elconf.remotes.push_back(elliptics_proxy_t::remote(host, port, family));
		elconf.success_copies_num = SUCCESS_COPIES_TYPE__ALL;

		proxy.reset(new elliptics_proxy_t(elconf));
		sleep(1);
	}

	void process(tests_t tests) {
		size_t num = 0;

		for (auto it = tests.begin(), end = tests.end(); it != end; ++it) {
			std::cout << "Test #" << ++num << ": ";
			try {
				(*it)(*proxy);
				std::cout << "Test #" << num << ": done" << std::endl;
			} catch (...) {
				std::cout << "Test #" << num << ": fail" << std::endl;
			}
		}
	}

private:
	std::shared_ptr<elliptics_proxy_t> proxy;
};

#define ADD_TEST(name, func) suite.addTest(new elliptics_caller_t(name, &elliptics_tests_t:: func, elliptics_tests))

int main(int argc, char* argv[])
{
	TestResult controller;
	std::unique_ptr<TestListener> listener(new JetBrains::TeamcityProgressListener());
	controller.addListener(listener.get());
	TestSuite suite;

	elliptics_tests_t elliptics_tests(std::string(argv[1]) + "/manager.sh");
	typedef TestCaller<elliptics_tests_t> elliptics_caller_t;

	ADD_TEST("test_write_g4_scnALL", test_write_g4_scnALL);
	ADD_TEST("test_write_g3_scnALL", test_write_g3_scnALL);
	ADD_TEST("test_write_g2_1_scnALL", test_write_g2_1_scnALL);
	ADD_TEST("test_write_g2_1_scnQUORUM", test_write_g2_1_scnQUORUM);
	ADD_TEST("test_write_g1_2_scnQUORUM", test_write_g1_2_scnQUORUM);
	ADD_TEST("test_write_g1_2_scnANY", test_write_g1_2_scnANY);

	suite.run(&controller);
	return 0;

	std::cout << "##teamcity[testSuiteStarted name='suite.name']" << std::endl;
	std::cout << "##teamcity[testStarted name='testname']" << std::endl;
	//<here go all the test service messages with the same name>
	std::cout << "##teamcity[testFinished name='testname']" << std::endl;
	std::cout << "##teamcity[testSuiteFinished name='suite.name']" << std::endl;
	return 0;
	std::string host = "localhost";
	int port = 1025;
	int family = 2;
	switch (argc) {
	case 4:
		family = boost::lexical_cast<int>(argv[3]);
	case 3:
		port = boost::lexical_cast<int>(argv[2]);
	case 2:
		host.assign(argv[1]);
		break;
	default:
		if (argc != 1) {
			std::cout << "Usage:" << std::endl
					  << std::cout << "name [host [port [family]]]" << std::endl;
		}
		return 0;
	}

	tester t(host, port, family);
	t.process({test_sync
			  , test_sync_embeds
			  , test_async
			  , test_bulk_sync
			  , test_lookup
			  , test_mastermind_groups
});
	return 0;
#if 0
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
		auto results = proxy.bulk_write (keys, data_arr, _groups = groups);
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
#endif
}

