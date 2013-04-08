Libelliptics-proxy provides you usefull development kit to communicate with elliptics. It consists of elliptics_proxy_t class and several ancillary enumerations and structures.

Libelliptics-proxy uses boost.parameters so you do not have to set long list of parameters to call a function.
There are list of required parameters (you have to set them to call a function):

- key_t key;	
Is used to identify data or the name of the application.

- std::vector<key_t> keys; 
Is used to identidy a set of data.

- key_t from, to;
Are used together to identidy a set of data.

- std::string data;
Keeps a binary data which should be stored or  should be send to the script.

- std::string script;
Is the name of the script of the application.


There are list of optional parameters (you do not have to set them to call a function, parameters will set with default values in this case). Some of the names are crossed with names in config and overlap them. Values from config will use if you do not set these parameters.

- std::vector<int> groups;
List of gtoups which will be used to store a data.

- uint64_t cflags;


- uint64_t ioflags;
Is a set of flags.
DNET_IO_FLAGS_APPEND - Append given data at the end of the object
DNET_IO_FLAGS_PREPARE - eblob prepare/commit phase
DNET_IO_FLAGS_COMMIT
DNET_IO_FLAGS_PLAIN_WRITE - this flag is used when we want backend not to perform any additional actions
except than write data at given offset. This is no-op in filesystem backend,
but eblob should disable prepare/commit operations.

- uint64_t size;
How many bytes to read or write.

- uint64_t offset;
The offset from the beginning of data.

- bool latest;


- uint64_t count;


- std::vector<std::shared_ptr<embed_t> embeds;
Additional info to data.

- bool embeded;
Set this flag if it needs to read embed data before general data.

- unsigned int success_copies_num;
Specify a number how many good recording is needed to consider write call as successful.
Positive value – count of good recording must be greater than or equal to this value.
SUCCESS_COPIES_TYPE__ANY – a successful recording is enough.
SUCCESS_COPIES_TYPE__QUORUM – requires [replication_count div 2 plus 1] successful records.
SUCCESS_COPIES_TYPE__ALL – exactly replication_count successful records are needed.

- uint64_t limit_start;


- uint64_t limit_num;



You have to fill elliptics_proxy_t::config for create an elliptics_proxy_t object. It consists:

- std::string log_path;
Path to the logfile.

- uint32_t log_mask;
Determines constraints which messages should not be printed. It can be set to: 
DNET_LOG_DATA, DNET_LOG_ERROR, DNET_LOG_INFO, DNET_LOG_NOTICE, DNET_LOG_DEBUG.
For example: if you set log_mask to DNET_LOG_INFO you will get DATA, ERROR and INFO messages, but NOTICE and DEBUG messages will not appear in logfile.

- std::vector<elliptics_proxy_t::remote> remotes;
List of remote nodes. Proxy will communicate with these nodes at first time to get a remote table.
elliptics_proxy_t::remote consists host name, port and family.

- int flags;
Specifies wether given node will join the network  or it is a client node and its ID should not be checked against collision with others. Also has a bit to forbid route list download.

- std::string ns;
Determines namespace.

- unsigned int wait_timeout;
Wait timeout in seconds used for example to wait for remote content sync.

- long check_timeout;
Wait until transaction acknowledge is received.

- std::vector<int> groups;
List of gtoups which will be used to store a data if you do not specify another.

- int base_port;


- int directory_bit_num;


- int success_copies_num;
Specify a number how many good recording is needed to consider write call as successful.
Positive value – count of good recording must be greater than or equal to this value.
SUCCESS_COPIES_TYPE__ANY – a successful recording is enough.
SUCCESS_COPIES_TYPE__QUORUM – requires [replication_count div 2 plus 1] successful records.
SUCCESS_COPIES_TYPE__ALL – exactly replication_count successful records are needed.


- int state_num;


- int replication_count;
How many replicas needs to be stored. Equals to size of groups list if not set.


- int chunk_size;
Data will be sent in packs of chunk_size bytes if chunk_size greater than zero.
Is not used when either DNET_IO_FLAGS_PREPARE or DNET_IO_FLAGS_COMMIT or DNET_IO_FLAGS_PLAIN_WRITE is set into ioflags.


- bool eblob_style_path;
Determines a representation of path to data.

- std::string cocaine_config;
Path to the cocaine config.


- int group_weights_refresh_period;
Time in milliseconds. Is used to wait between requests to mastermind.

Example:
```
elliptics_proxy_t::config c;
c.groups.push_back(1);
c.groups.push_back(2);
c.log_mask = DNET_LOG_ERROR;
c.remotes.push_back(elliptics_proxy_t::remote("localhost", 1025, 2));
c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;
elliptics_proxy_t proxy(c);
```


Elliptics_proxy_t members list:

lookup_result_t lookup(key_t &key, std::vector<int> &groups);

std::vector<lookup_result_t> write(key_t &key, std::string &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);

read_result_t read(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);

void remove(key_t &key, std::vector<int> &groups);

std::vector<std::string> range_get(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags, uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);

std::map<key_t, read_result_t> bulk_read(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);

std::vector<elliptics_proxy_t::remote> lookup_addr(key_t &key, std::vector<int> &groups);

std::map<key_t, std::vector<lookup_result_t> > bulk_write(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags, std::vector<int> &groups, unsigned int success_copies_num);

std::string exec_script(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);

async_read_result_t read_async(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);

async_write_result_t write_async(key_t &key, std::string &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);

async_remove_result_t remove_async(key_t &key, std::vector<int> &groups);


Example:
```
key_t k(std::string("filename.txt"));
proxy.remove (k);
std::string data("some data");
std::vector <int> g = {2};
std::vector<lookup_result_t> l = proxy.write(k, data, _groups = g);
std::cout << "written " << l.size() << " copies" << std::endl;
for (auto it = l.begin(); it != l.end(); ++it) {
    std::cout << "\tpath: " << it->hostname << ":" << it->port << it->path << std::endl;
}
lookup_result_t l1 = proxy.lookup(k);
std::cout << "lookup path: " << l1.hostname << ":" << l1.port << l1.path << std::endl;
```


_async suffix means method is asynchronous. You get async_object after call that and can to call method 'get' to get a result.

Example:
```
key_t k1(std::string("key1.txt"));
key_t k2(std::string("key2.txt"));

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

try { proxy.remove_async (k1).get (); } catch (...) {}
try { proxy.remove_async (k2).get (); } catch (...) {}
```

