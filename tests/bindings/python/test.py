# coding=utf-8
import sys
sys.path.append("bindings/python/")
sys.path.append("/usr/local/lib/python2.7/dist-packages/teamcity_messages-1.7-py2.7.egg")
print sys.path

from teamcity import is_running_under_teamcity
from teamcity.unittestpy import TeamcityTestRunner
from elliptics_proxy import *

import unittest

r = remote('localhost', 1025)
c = config()
c.groups.append(1)
c.groups.append(2)
c.log_path = 'log'
c.remotes.append(r)
proxy = elliptics_proxy_t(c)

class TestTeamcityMessages(unittest.TestCase):

	def test_sync(self):
		data = 'test data of test_sync'
		key = 'test_sync_key'
		lr = proxy.write(key, data)
		assert len(lr) == 2
		dc2 = proxy.read(key)
		assert dc2.data == data
		proxy.remove(key)

	def test_async(self):
		data = 'test data for test_async'
		key = 'test_async_key'
		dc = data_container_t(data)
		dc.timestamp = timespec(123, 456789)

		awr = proxy.write_async(key, dc)
		lr = awr.get()
		assert len(lr) == 2
			
		arr = proxy.read_async(key, embeded = True, groups = [1]);
		ldc = arr.get()
		dc = ldc[0]
		assert dc.data == data
		ts2 = dc.timestamp
		assert ts2.tv_sec == 123
		assert ts2.tv_nsec == 456789
			
		arr = proxy.read_async(key, embeded = True, groups = [2]);
		ldc = arr.get()
		dc = ldc[0]
		assert dc.data == data
		ts2 = dc.timestamp
		assert ts2.tv_sec == 123
		assert ts2.tv_nsec == 456789
			
		arm = proxy.remove_async(key);
		arm.wait()

if __name__ == '__main__':
	runner = TeamcityTestRunner()
	unittest.main(testRunner=runner)
