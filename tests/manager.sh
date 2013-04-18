#!/bin/bash

gpath=`pwd`/nodes;

function check_node {
	g=1
	while [ "$g" -eq 1 ]; do
		dnet_ioclient -r localhost:$((1024 + $1)):2 2> manager.out
		res=$?
		echo $res
		# starting
		if [ "$2" -eq 1 ] && [ "$res" -eq 0 ]; then 
			g=0;
		elif [ "$2" -eq 2 ] && [ "$res" -ne 0 ]; then
			g=0;
		fi
	done
}

function start_node {
	path=$gpath/$1
	if ! [ -f $path/pid ]; then
		dnet_ioserv -c $path/elliptics.conf &
		echo $! >  $path/pid
		check_node $1 1;
	fi
}

function stop_node {
	path=$gpath/$1
	if [ -f $path/pid ]; then
		kill `cat $path/pid`
		rm $path/pid
		check_node $1 2;
	fi
}

function prepare_env {
	apt-get install elliptics
	for index in `seq 1 3`; do
		path=$gpath/$index;
		rm -rf $path;
		mkdir -p $path;
		mkdir $path/data;
		mkdir $path/kdb;
		filename="$path/elliptics.conf";
		echo "" > $filename;
		echo "log = $path/log" >> $filename;
		echo "group = $index" >> $filename;
		echo "history = $path/kdb" >> $filename;
		echo "io_thread_num = 250" >> $filename;
		echo "net_thread_num = 100" >> $filename;
		echo "nonblocking_io_thread_num = 50" >> $filename;
		echo "log_level = 2" >> $filename;
		echo "join = 1" >> $filename;
		echo "remote = localhost:1025:2" >> $filename;
		echo "addr = localhost:$((1024+$index)):2" >> $filename;
		echo "wait_timeout = 30" >> $filename;
		echo "check_timeout = 50" >> $filename;
		echo "auth_cookie = unique_storage_cookie" >> $filename;
		echo "cache_size =" >> $filename;
		echo "server_net_prio = 0x20" >> $filename;
		echo "client_net_prio = 6" >> $filename;
		echo "flags = 8" >> $filename;
		echo "backend = blob" >> $filename;
		echo "blob_size = 10G" >> $filename;
		echo "records_in_blob = 5000000" >> $filename;
		echo "blob_flags = 2" >> $filename;
		echo "blob_cache_size = 0" >> $filename;
		echo "defrag_timeout = 3600" >> $filename;
		echo "defrag_percentage = 25" >> $filename;
		echo "sync = 30" >> $filename;
		echo "data = $path/data" >> $filename;
	done;
}

function start_nodes {
	if [ "$#" -eq 1 ]; then
		for index in `seq 1 3`; do
			start_node $index;
		done;
	elif [ "$2" -ge 1 ] && [ "$2" -le 3 ]; then
		start_node $2;
	fi 
}

function stop_nodes {
	if [ "$#" -eq 1 ]; then
		for index in `seq 1 3`; do
			stop_node $index;
		done;
	elif [ "$2" -ge 1 ] && [ "$2" -le 3 ]; then
		stop_node $2;
	fi 
}

function clear_env {
	stop_nodes $@;
	rm -rf $gpath;
}

if [ "$#" -lt 1 ]; then
	echo not enough args
	exit 1
fi

if test $1 = "prepare"; then
	clear_env $@;
	prepare_env $@;
	exit 0;
elif test $1 = "clear"; then
	clear_env $@;
	exit 0;
elif test $1 = "start"; then
	start_nodes $@;
	exit 0;
elif test $1 = "stop"; then
	stop_nodes $@;
	exit 0;
fi

echo unknown command
exit 2
