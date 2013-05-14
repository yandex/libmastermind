Libelliptics-proxy provides you usefull development kit to communicate with elliptics. It consists of `elliptics_proxy_t` class and several ancillary enumerations and structures. Methods of libelliptics-proxy have long lists of parameters but libelliptcis-proxy uses boost.parameters so you do not have to set all of them to call a function. Some of the names of parameters are crossed with names in config and overlap them - values of config will use if you do not set relate parameters.

# Content
- [Support structures](#-support-structures)
	- [remote](#-remote)
	- [config](#-config)
	- [key_t](#-key_t)
	- [data_container_t](#-data_container_t)
	- [lookup_result_t](#-lookup_result_t)
	- status_result_t
	- [async_read_result_t](#-async_read_result_t)
	- [async_write_result_t](#-async_write_result_t)
	- [async_remove_result_t](#-async_remove_result_t)
- [Parameters](#-parameters)
	- [Required](#-required)
	- [Optional](#-optional)
- [Proxy methods](#-proxy-methods)
	- [Synchronous](#-synchronous)
		- [lookup](#-lookup)
		- [write](#-write)
		- [read](#-read)
		- [remove](#-remove)
		- [range_get](#-range_get)
		- [bulk_read](#-bulk_read)
		- [lookup_addr](#-lookup_addr)
		- [bulk_write](#-bulk_write)
		- [exec_script](#-exec_script)
		- [ping](#-ping)
		- [stat_log](#-stat_log)
		- [get_metabalancer_groups](#-get_metabalancer_groups)
		- [get_metabalancer_group_info](#-get_metabalancer_group_info)
		- [get_symmetric_groups](#-get_symmetric_groups)
		- [get_bad_groups](#-get_bad_groups)
		- [get_all_groups](#-get_all_groups)
	- [Asynchronous](#-asynchronous)
		- [read_async](#-read_async)
		- [write_async](#-write_async)
		- [remove_async](#-remove_async)
- [Examples](#-examples)
	- [Initialization](#-initialization)
	- [Synchronous write/lookup](#-synchronous-writelookup)
	- [Asynchronous write/read](#-asynchronous-writeread)

# <a id="support-structures"/> Support structures

## <a id="remote"/> remote
Provides information about remote host.  
Declares in the elliptics_proxy_t.  
Fields:  

|Type|Name|Description|
|----|----|-----------|
|std::string|host|host name|
|int|port| |
|int|family|Default value: 2|

Example:
```cpp
elliptics_proxy_t::remote r("localhost", 1025);
```

## <a id="config"/> config
Declares in the elliptics_proxy_t.  
Fields:  

|Type|Name|Description|
|----|----|-----------|
|std::string|log_path|Path to the logfile. Default value: "/dev/stderr".|
|uint32_t|log_mask|Determines constraints which messages should not be printed. It can be set to: <br> - DNET_LOG_DATA <br> - DNET_LOG_ERROR <br> - DNET_LOG_INFO <br> - DNET_LOG_NOTICE <br> - DNET_LOG_DEBUG <br>For example: if you set log_mask to DNET_LOG_INFO you will get DATA, ERROR and INFO messages, but NOTICE and DEBUG messages will not appear in logfile.Default value: DNET_LOG_INFO with DNET_LOG_ERROR.|
|std::vector<elliptics_proxy_t::remote>|remotes|List of remote nodes. Proxy will communicate with these nodes at first time to get a remote table. Default value: empty vector.|
|int|flags|Specifies wether given node will join the network, or it is a client node and its ID should not be checked against collision with others. Also has a bit to forbid route list download. Default value: 0|
|unsigned int|wait_timeout|Wait timeout in seconds used for example to wait for remote content sync. Default value: 0|
|long|check_timeout|Wait until transaction acknowledge is received. Default value: 0|
|std::vector<int>|groups|List of groups which will be used to store a data if you do not specify another with relate optional parameter. Default value: empty vector.|
|int|base_port|Default value: 1024.|
|int|directory_bit_num|Default value: 32.|
|int|success_copies_num|Specify a number how many good recording is needed to consider write call as successful. <br> - Positive value <br> count of good recording must be greater than or equal to this value. <br> - SUCCESS_COPIES_TYPE__ANY <br> a successful recording is enough. <br> - SUCCESS_COPIES_TYPE__QUORUM <br> requires [replication_count div 2 plus 1] successful records. <br> - SUCCESS_COPIES_TYPE__ALL <br> exactly replication_count successful records are needed. <br> Default value: 2.|
|int|die_limit|Maximum valid number of inaccessible dnet backends. Default value: 0.|
|int|replication_count|How many replicas should be stored. Default value: 0.|
|int|chunk_size|Data will be sent in packs of chunk_size bytes if chunk_size greater than zero. Is not used when either DNET_IO_FLAGS_PREPARE or DNET_IO_FLAGS_COMMIT or DNET_IO_FLAGS_PLAIN_WRITE is set into ioflags. Default value: 0.|
|bool|eblob_style_path|Determines a representation of path to data. Default value: true.|
|std::string|cocaine_config|Path to the cocaine config. Default value: empty string.|
|int|group_weights_refresh_period|Time in seconds. Is used to wait between requests to mastermind. Default value: 60.|

## <a id="key_t"/> key_t
Is used to identify a written data.  
There are two ways to construct a key_t:
- name (std::string) and type (int, default = 0)
- dnet_id  

Example for first way:  
```cpp
elliptcis::key_t key("filename.txt");
```

dnet_id is identifier which is used in elliptcis:
```cpp
struct dnet_id {
	uint8_t id[DNET_ID_SIZE];
	uint32_t group_id;
	int type;
} __attribute__ ((packed));
```
Example for second way:
```cpp
ioremap::elliptics::dnet_id id;
// fill fields of id here
elliptics::key_t key(id);
```
Methods of key_t:  

|Declaration|Description|
|-----------|-----------|
|`bool by_id() const;`|Returns true if key was constructed by dnet_id.|
|`const std::string &remote() const;`|Returns name of key if it was constructed by first way.|
|`int type() const;`|Returns of key if it was constructed by first way.|
|`const dnet_id &id() const;`|Returns dnet_id of key if it was constructed by second way.|
|`std::string to_string() const;`|Returns string representation of key.|

## <a id="data_container_t"/> data_container_t
This class provides useful way to store data with additional (embedded) information.  
Example:  
```cpp
std::string data("test data");
timespec ts;
ts.tv_sec = 123;
ts.tv_nsec = 456789;
data_container_t dc;
dc.set(data);
dc.set<DNET_FCGI_EMBED_TIMESTAMP>(ts);
```
Stores some data and additional timestamp in data container, then `dc` can be written.  
You can write a std::string if you do not need embeded information - `data_container_t` has implicit constructor of std::string.  
Also you can register your own types for additional inforamtion:
```cpp
enum my_embed_types {
	  met_simple_type = 1025
	, met_complex_type = 1026
};

template<> struct type_traits<met_simple_type> : type_traits_base<simple_type> {};

template<> struct type_traits<met_complex_type> : type_traits_base<complex_type> {
	static ioremap::elliptics::data_pointer convert(const type &ob) {
		/*
		You can write own converter type -> raw-data
		*/
		ioremap::elliptics::data_buffer data_buffer(sizeof(ob.field_size) * 2);
		data_buffer.write(ob.field1);
		data_buffer.write(std::abs(ob.field2));
		return std::move(data_buffer);
	}

	static type convert(ioremap::elliptics::data_pointer data_pointer) {
		/*
		You can write own converter raw-data -> data
		*/
		type res;
		
		read(data_pointer, res.field1);
		read(data_pointer, res.field2);

		return res;
	}
};
```
You can either use standart packing system (msgpack) for simple types or reimplement own converter methods for complex types.
Then you can add embedded information with your type:
```cpp
simple_type ob1;
complex_type ob2;
dc.set<met_simple_type>(ob1);
dc.set<met_complex_type>(ob2);
```
Use get method to extract data:
```cpp
std::string data = dc.get();
```
To extract additional information use:
```cpp
boost::optional<simple_type> ob = dc.get<met_simple_type>();
if (ob) {
	// met_simple_type exists in dc
	// to get data use either operator * or operator ->
} else {
	// met_simple_typy does not exist in dc
}
```

## <a id="lookup_result_t"/> lookup_result_t
This class provides information about written replica.
All methods use "lazy" semmantic - inforamtion will resolved only if you call a corresponding method.
Example:
```cpp
lookup_result_t lr = proxy->lookup(key);
uint16_t port = lr.port();
```
Only port will be resolved in this case.

Methods list:
- host
- port
- group
- status
- addr
- path
- full_path

## <a id="status_result_t"/> status\_result\_t

## <a id="async_read_result_t"/> async\_read\_result\_t
This class implements a 'future' semantics for read_async.
You can gain result (data_container_t) by method `get()`. This method will wait result until it is not ready to read.

## <a id="async_write_result_t"/> async\_write\_result\_t
This class implements a 'future' semantics for write_async.
You can gain result: either std::vector<lookup_result_t> by method `get()` or first lookup_result_t by method `get_one()`. These methods will wait result until it is not ready to read.

## <a id="async_remove_result_t"/> async\_remove\_result\_t
This class implements a 'future' semantics for remove_async.
You can wait until the method is not done by `wait()`.

# <a id="parameters"/> Parameters

## <a id="required"/> Required
There are list of required parameters (you have to set them to call a function):

- `key_t key;`  
Is used to identify data or the name of the application.
- `std::vector<key_t> keys;`  
Is used to identidy a set of data.
- `key_t from, to;`  
Are used together to identidy a set of data.
- `std::string data;` or `data_container_t data;`  
Keeps a binary data which should be stored or should be sent to the script.
- `std::string script;`  
Is the name of the script of the application.

## <a id="optional"/> Optional
There are list of optional parameters (you do not have to set them to call a function, parameters will set with default values in this case). Some of the names are crossed with names in config and overlap them. Values from config will use if you do not set these parameters.

- `std::vector<int> groups;`  
List of gtoups which will be used to store a data.
- `uint64_t cflags;`
- `uint64_t ioflags;`  
Is a set of flags.
    - DNET\_IO\_FLAGS\_APPEND  
    Append given data at the end of the object
    - DNET\_IO\_FLAGS\_PREPARE  
    eblob prepare phase
    - DNET\_IO\_FLAGS\_COMMIT  
    eblob commit phase
    - DNET\_IO\_FLAGS\_PLAIN\_WRITE  
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

# <a id="proxy-methods"/> Proxy methods

## <a id="synchronous"/> Synchronous

### <a id="lookup"/> lookup

```cpp
lookup_result_t lookup(key_t &[key](#-key), std::vector<int> &groups);
```

Looks up record with [key](#-required) in [groups](#-optional).

**Return value**

Returns [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

### <a id="write"/> write

```cpp
std::vector<lookup_result_t> write(key_t &key, data_container_t &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num);
```

Try to write [data](#-required) by [key](#-required) into [groups](#-optional). Remove written records if their number does not relate with [success_copies_num](#-optional).  
Throws std::exception if failure occurs.

**Return value**

Returns vector of [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

### <a id="read"/> read

```cpp
data_container_t read(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);
```

Try to find record with [key](#-required) in [groups](#-optional).  
Throws std::exception if failure occurs.


**Return value**

Returns [`data_container_t`](#-data_container_t).

<!-- [Methods](#-methods) -->

### <a id="remove"/> remove

```cpp
void remove(key_t &key, std::vector<int> &groups);
```

Remove record with [key](#-required) from [groups](#-optional).
Throws std::exception if record not found.

**Return value**

None.

<!-- [Methods](#-methods) -->

### <a id="range_get"/> range_get

```cpp
std::vector<std::string> range_get(key_t &from, key_t &to, uint64_t cflags, uint64_t ioflags, uint64_t limit_start, uint64_t limit_num, const std::vector<int> &groups, key_t &key);
```

Try to read compact records pack between keys [from](#-required) and [to](#-required).  
Throws std::exception if failure occurs.

**Return value**

Returns vector of std::strings.

<!-- [Methods](#-methods) -->

### <a id="bulk_read"/> bulk_read

```cpp
std::map<key_t, data_container_t> bulk_read(std::vector<key_t> &keys, uint64_t cflags, std::vector<int> &groups);
```

Try to read records pack for the set of [keys](#-required).
Throws std::exception if failure occurs.  
`bulk_read` is more effective than serial calls of [`read`](#-read) but gains the same result.

**Return value**

Returns maps of [`key_t`](#-key_t) and [`data_container_t`](#-data_container_t).

<!-- [Methods](#-methods) -->

### <a id="lookup_addr"/> lookup_addr

```cpp
std::vector<elliptics_proxy_t::remote> lookup_addr(key_t &key, std::vector<int> &groups);
```

**Return value**

Returns vector of maps of [`elliptics_proxy_t::remote`](#-remote).

<!-- [Methods](#-methods) -->

### <a id="bulk_write"/> bulk_write

```cpp
std::map<key_t, std::vector<lookup_result_t> > bulk_write(std::vector<key_t> &keys, std::vector<data_container_t> &data, uint64_t cflags, std::vector<int> &groups, unsigned int success_copies_num);
```

Try to write a set of [data](#-required) with the set of [keys](#-required).  
Throws std::exception if failure occurs.  
`bulk_write` is more effective than serial calls of [`write`](#-write) but gains the same result.

**Return value**

Returns maps of [`key_t`](#-key_t) and vector of [`lookup_result_t`](#-lookup_result_t).

<!-- [Methods](#-methods) -->

### <a id="exec_script"/> exec_script

```cpp
std::string exec_script(key_t &key, std::string &data, std::string &script, std::vector<int> &groups);
```

**Return value**

Returns string.

<!-- [Methods](#-methods) -->

### <a id="ping"/> ping

```cpp
bool ping();
```

Check whether or not count of connections greter then or equal to [die_limit](#initialization).

**Return value**

Returns boolean.

<!-- [Methods](#-methods) -->

### <a id="stat_log"/> stat_log

```cpp
std::vector<status_result_t> stat_log();
```

Collects information about nodes from the remote table.

**Return value**

Returns vector of [`status_result_t`](#-status_result_t).

<!-- [Methods](#-methods) -->

### <a id="get_metabalancer_groups"/> get_metabalancer_groups
```cpp
std::vector<int> get_metabalancer_groups(uint64_t count, uint64_t size, key_t &key);
```

### <a id="get_metabalancer_group_info"/> get_metabalancer_group_info
```cpp
group_info_response_t get_metabalancer_group_info(int group);
```

### <a id="get_symmetric_groups"/> get_symmetric_groups
```cpp
std::vector<std::vector<int> > get_symmetric_groups();
```

### <a id="get_bad_groups"/> get_bad_groups
```cpp
std::map<int, std::vector<int> > get_bad_groups();
```

### <a id="get_all_groups"/> get_all_groups
```cpp
std::vector<int> get_all_groups();
```


## <a id="asynchronous"/> Asynchronous


`_async` suffix means method is asynchronous. You get async_object after call that and can to call method 'get' to get a result.

### <a id="read_async"/> read_async

```cpp
async_read_result_t read_async(key_t &key, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, bool latest, bool embeded);
```

Sends request for [`read`](#-read) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_read_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

### <a id="write_async"/> write_async

```cpp
async_write_result_t write_async(key_t &key, data_container_t &data, uint64_t offset, uint64_t size, uint64_t cflags, uint64_t ioflags, std::vector<int> &groups, unsigned int success_copies_num, std::vector<std::shared_ptr<embed_t> > embeds);
```

Sends request for [`write`](#-write) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_write_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

### <a id="remove_async"/> remove_async

```cpp
async_remove_result_t remove_async(key_t &key, std::vector<int> &groups);
```

Sends request for [`remove`](#-remove) to elliptics and returns control to your thread immediately.  
You can gain a result by [`async_remove_result_t`](#-async_result) later.

**Return value**

Returns [`async_result`](#-async_result).

<!-- [Methods](#-methods) -->

# <a id="examples"/> Examples

## <a id="initialization"/> Initialization
You have to create and fill [`elliptics_proxy_t::config`](#-config) to create an `elliptics_proxy_t` object:  
```cpp
elliptics_proxy_t::config elconf;
```
You need set remotes at least:  
```cpp
elconf.remotes.push_back(elliptics_proxy_t::remote("localhost", 1025));
```
A remote item is enough to be able to get a remote table.  
Accomplished example of initialization:  
```cpp
elliptics_proxy_t::config elconf;
elconf.groups.push_back(1);
elconf.groups.push_back(2);
elconf.log_mask = DNET_LOG_ERROR;
elconf.remotes.push_back(elliptics_proxy_t::remote("localhost", 1025));
elconf.success_copies_num = SUCCESS_COPIES_TYPE__ANY;
elliptics_proxy_t proxy(elconf);
```
In this case proxy will communicate with elliptcis node (localhost:1025) and gain remote table from that. Proxy will use groups 1 and 2 to write a data if it does not specify another by relate optional parameter. Only record is needed to consider a write as successful (also if it does not specify another by relate optional parameter). Only error messages will appear in /dev/stderr.

## <a id="synchronous-writelookup"/> Synchronous write/lookup
```cpp
elliptics::key_t k("filename.txt");
std::string data("some data");
std::vector <int> g = {2};
std::vector<lookup_result_t> l = proxy.write(k, data, _groups = g);
std::cout << "written " << l.size() << " copies" << std::endl;
for (auto it = l.begin(); it != l.end(); ++it) {
	std::cout << "\tpath: " << it->host() << ':' << it->port() << it->path() << std::endl;
}
lookup_result_t l1 = proxy.lookup(k);
std::cout << "lookup path: " << l1.host() << ':' << l1.port() << l1.path()  << std::endl;
```

## <a id="asynchronous-writeread"/> Asynchronous write/read
```cpp
elliptics::key_t k1("key1.txt");
elliptics::key_t k2("key2.txt");

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
	std::cout << "\tpath: " << it->host() << ':' << it->port() << it-   >path() << std::endl;
}

try {
	l = awr2.get ();
} catch (...) {
	std::cout << "Exception during get write2 result" << std::endl;
	return;
}
std::cout << "written " << l.size() << " copies" << std::endl;
for (auto it = l.begin(); it != l.end(); ++it) {
	std::cout << "\tpath: " << it->host() << ':' << it->port() << it-   >path() << std::endl;
}

async_read_result_t arr1 = proxy.read_async(k1);
async_read_result_t arr2 = proxy.read_async(k2);
data_container_t dc;

try {
	dc = arr1.get ();
} catch (...) {
	std::cout << "Exception during get read result" << std::endl;
	return;
}
std::cout << "Read result: " << dc.data.to_string() << std::endl;

try {
	dc = arr2.get ();
} catch (...) {
	std::cout << "Exception during get read result" << std::endl;
	return;
}
std::cout << "Read result: " << dc.data.to_string() << std::endl;
```

