Libelliptics-proxy provides you usefull development kit to communicate with elliptics. It consists of `elliptics_proxy_t` class and several ancillary enumerations and structures.

# <a id="parameters"/> Parameters

## <a id="parameters"/> Required
Libelliptics-proxy uses boost.parameters so you do not have to set long list of parameters to call a function.
There are list of required parameters (you have to set them to call a function):

- `key_t key;`  
Is used to identify data or the name of the application.
- `std::vector<key_t> keys;`  
Is used to identidy a set of data.
- `key_t from, to;`  
Are used together to identidy a set of data.
- `std::string data;`  
Keeps a binary data which should be stored or should be send to the script.
- `std::string script;`  
Is the name of the script of the application.

## <a id="optional"/> Optional
There are list of optional parameters (you do not have to set them to call a function, parameters will set with default values in this case). Some of the names are crossed with names in config and overlap them. Values from config will use if you do not set these parameters.

- `std::vector<int> groups;`  
List of gtoups which will be used to store a data.
- `uint64_t cflags;`
- `uint64_t ioflags;`  
Is a set of flags.
    - DNET_IO_FLAGS_APPEND  
    Append given data at the end of the object
    - DNET_IO_FLAGS_PREPARE  
    eblob prepare phase
    - DNET_IO_FLAGS_COMMIT  
    eblob commit phase
    - DNET_IO_FLAGS_PLAIN_WRITE  
    this flag is used when we want backend not to perform any additional actions
    except than write data at given offset. This is no-op in filesystem backend,
    but eblob should disable prepare/commit operations.
- `uint64_t size;`  
How many bytes to read or write.
- `uint64_t offset;`  
The offset from the beginning of data.
- `bool latest;`
- `uint64_t count;`
- `std::vector<std::shared_ptr<embed_t> embeds;`  
Additional info to data.
- `bool embeded;`   
Set this flag if it needs to read embed data before general data.
- `unsigned int success_copies_num;`  
Specify a number how many good recording is needed to consider write call as successful.
    - Positive value  
    count of good recording must be greater than or equal to this value.
    - SUCCESS_COPIES_TYPE__ANY  
    a successful recording is enough.
    - SUCCESS_COPIES_TYPE__QUORUM  
    requires [replication_count div 2 plus 1] successful records.
    - SUCCESS_COPIES_TYPE__ALL  
    exactly replication_count successful records are needed.
- `uint64_t limit_start;`
- `uint64_t limit_num;`

#Initialization
You have to fill `elliptics_proxy_t::config` for create an `elliptics_proxy_t` object. It consists:

- `std::string log_path;`  
Path to the logfile.
- `uint32_t log_mask;`  
Determines constraints which messages should not be printed. It can be set to: 
    - DNET_LOG_DATA
    - DNET_LOG_ERROR
    - DNET_LOG_INFO
    - DNET_LOG_NOTICE
    - DNET_LOG_DEBUG  
For example: if you set log_mask to DNET_LOG_INFO you will get DATA, ERROR and INFO messages, but NOTICE and DEBUG messages will not appear in logfile.
- `std::vector<elliptics_proxy_t::remote> remotes;`  
List of remote nodes. Proxy will communicate with these nodes at first time to get a remote table.
`elliptics_proxy_t::remote` consists host name, port and family.
- `int flags;`  
Specifies wether given node will join the network  or it is a client node and its ID should not be checked against collision with others. Also has a bit to forbid route list download.
- `std::string ns;`  
Determines namespace.
- `unsigned int wait_timeout;`  
Wait timeout in seconds used for example to wait for remote content sync.
- `long check_timeout;`  
Wait until transaction acknowledge is received.
- `std::vector<int> groups;`  
List of gtoups which will be used to store a data if you do not specify another.
- `int base_port;`
- `int directory_bit_num;`
- `int success_copies_num;`  
Specify a number how many good recording is needed to consider write call as successful.
    - Positive value  
    count of good recording must be greater than or equal to this value.
    - SUCCESS_COPIES_TYPE__ANY  
    a successful recording is enough.
    - SUCCESS_COPIES_TYPE__QUORUM  
    requires [replication_count div 2 plus 1] successful records.
    - SUCCESS_COPIES_TYPE__ALL  
    exactly replication_count successful records are needed.
- `int die_limit;`
- `int replication_count;`  
How many replicas needs to be stored. Equals to size of groups list if not set.
- `int chunk_size;`  
Data will be sent in packs of chunk_size bytes if chunk_size greater than zero.
Is not used when either DNET_IO_FLAGS_PREPARE or DNET_IO_FLAGS_COMMIT or DNET_IO_FLAGS_PLAIN_WRITE is set into ioflags.
- `bool eblob_style_path;`  
Determines a representation of path to data.
- `std::string cocaine_config;`  
Path to the cocaine config.
- `int group_weights_refresh_period;`  
Time in milliseconds. Is used to wait between requests to mastermind.

###Example:
```
elliptics_proxy_t::config c;
c.groups.push_back(1);
c.groups.push_back(2);
c.log_mask = DNET_LOG_ERROR;
c.remotes.push_back(elliptics_proxy_t::remote("localhost", 1025, 2));
c.success_copies_num = SUCCESS_COPIES_TYPE__ANY;
elliptics_proxy_t proxy(c);
```

# <a id="methods"/> Methods

Elliptics_proxy_t members list:

<!-- |Function|Description|
|-----|--------|
|[lookup](#-lookup)||
|[write](#-write)||
|[read](#-read)||
|[remove](#-remove)||
|[range_get](#-range_get)||
|[bulk_read](#-bulk_read)||
|[lookup_addr](#-lookup_addr)||
|[bulk_write](#-bulk_write)||
|[exec_script](#-bulk_write)||
|[read_async](#-read_async)||
|[write_async](#-write_async)||
|[remove_async](#-remove_async)|| -->

## <a id="lookup"/> lookup

```
lookup_result_t lookup(key_t &[key](#-key), std::vector<int> &groups);
```

Looks up record with [key](#-required) in [groups](#-optional).

**Return value**

Returns [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

## <a id="write"/> write

```
std::vector<lookup_result_t> write(key_t &key, std::string &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);
```

Try to write [data](#-required) by [key](#-required) into [groups](#-optional). Remove written records if their number does not relate with [success_copies_num](#-optional).  
Throws std::exception if failure occurs.

**Return value**

Returns vector of [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

## <a id="read"/> read

```
read_result_t read(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);
```

Try to find record with [key](#-required) in [groups](#-optional).  
Throws std::exception if failure occurs.


**Return value**

Returns [`read_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

## <a id="remove"/> remove

```
void remove(key_t &key, std::vector<int> &groups);
```

Remove record with [key](#-required) from [groups](#-optional).
Throws std::exception if record not found.

**Return value**

None.

<!-- [Methods](#-methods) -->

## <a id="range_get"/> range_get

```
std::vector<std::string> range_get(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags, uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);
```

Try to read compact records pack between keys [from](#-required) and [to](#-required).  
Throws std::exception if failure occurs.

**Return value**

Returns vector of std::strings.

<!-- [Methods](#-methods) -->

## <a id="bulk_read"/> bulk_read

```
std::map<key_t, read_result_t> bulk_read(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);
```

Try to read records pack for the set of [keys](#-required).
Throws std::exception if failure occurs.  
`bulk_read` is more effective than serial calls of [`read`](#-read) but gains the same result.

**Return value**

Returns maps of [`key_t`](#-key_t) and [`read_result_t`](#-read_result_t).

<!-- [Methods](#-methods) -->

## <a id="lookup_addr"/> lookup_addr

```
std::vector<elliptics_proxy_t::remote> lookup_addr(key_t &key, std::vector<int> &groups);
```

**Return value**

Returns vector of maps of [`elliptics_proxy_t::remote`](#-remote).

<!-- [Methods](#-methods) -->

## <a id="bulk_write"/> bulk_write

```
std::map<key_t, std::vector<lookup_result_t> > bulk_write(std::vector<key_t> &keys, std::vector<std::string> &data, uint64_t cflags, std::vector<int> &groups, unsigned int success_copies_num);
```

Try to write a set of [data](#-required) with the set of [keys](#-required).  
Throws std::exception if failure occurs.  
`bulk_write` is more effective than serial calls of [`write`](#-write) but gains the same result.

**Return value**

Returns maps of [`key_t`](#-key_t) and vector of [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

## <a id="exec_script"/> exec_script

```
std::string exec_script(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);
```

**Return value**

Returns string.

<!-- [Methods](#-methods) -->

## <a id="read_async"/> read_async

```
async_read_result_t read_async(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);
```

Sends request for [`read`](#-read) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_read_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

## <a id="write_async"/> write_async

```
async_write_result_t write_async(key_t &key, std::string &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);
```

Sends request for [`write`](#-write) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_write_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

## <a id="remove_async"/> remove_async

```
async_remove_result_t remove_async(key_t &key, std::vector<int> &groups);
```

Sends request for [`remove`](#-remove) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_remove_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

## <a id="ping"/> ping

```
bool ping();
```

Check whether or not count of connections greter then or equal to [die_limit](#initialization).

**Return value**

Returns boolean.

<!-- [Methods](#-methods) -->

## <a id="stat_log"/> stat_log

```
std::vector<status_result_t> stat_log();
```

Collects information about nodes from the remote table.

**Return value**

Returns vector of [`status_result_t`](#-status_result_t).

<!-- [Methods](#-methods) -->

###Example:
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


`_async` suffix means method is asynchronous. You get async_object after call that and can to call method 'get' to get a result.

###Example:
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

# <a id="types"/> Types

<!-- |Type|Description|
|-----|--------|
|[lookup_result_t](#-lookup_result_t)||
|[read_result_t](#-read_result_t)||
|[async_result](#-async_result)||
-->
## <a id="key_t"/> key_t

```
typedef ioremap::elliptics::key key_t;
```

<!-- [Types](#-types) -->

## <a id="lookup_result_t"/> lookup_result_t

```
class lookup_result_t {
public:
    std::string hostname;
	uint16_t port;
	std::string path;
	int group;
	int status;
	std::string addr;
	std::string short_path;
};
```

<!-- [Types](#-types) -->

## <a id="read_result_t"/> read_result_t

```
class read_result_t {
public:
    std::string data;
	std::vector<std::shared_ptr<embed_t> > embeds;
};
```

<!-- [Types](#-types) -->

## <a id="async_result"/> async_result

```
template<typename R, typename A>
class async_result {
public:
    typedef ioremap::elliptics::waiter<A> waiter_t;
	typedef std::function<A()> waiter2_t;
	typedef std::function<R(const A &)> parser_t;

private:
	struct wraper_t {
		wraper_t(const waiter_t &waiter)
			: waiter(waiter) {
		}

		A operator () () {
			return waiter.result();
		}

	private:
		waiter_t waiter;
	};

public:

	async_result(const waiter_t &waiter, const parser_t &parser)
		: waiter(wraper_t(waiter)), parser(parser)
	{}

	async_result(const waiter2_t &waiter, const parser_t &parser)
		: waiter(waiter), parser(parser)
	{}

	R get() {
		return parser(waiter());
	}

private:
	waiter2_t waiter;
	parser_t parser;
};

typedef async_result<read_result_t, ioremap::elliptics::read_result> async_read_result_t;
typedef async_result<std::vector<lookup_result_t>, ioremap::elliptics::write_result> async_write_result_t;
typedef async_result<void, std::exception_ptr> async_remove_result_t;
```

<!-- [Types](#-types) -->

## <a id="status_result_t"/> status_result_t

```
class status_result_t {
public:
    std::string addr;
	std::string id;
	float la [3];
	uint64_t vm_total;
	uint64_t vm_free;
	uint64_t vm_cached;
	uint64_t storage_size;
	uint64_t available_size;
	uint64_t files;
	uint64_t fsid;
};
```

<!-- [Types](#-types) -->

