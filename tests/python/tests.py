#!/usr/bin/env python
# -*- encoding: utf-8 -*-

# Must assure that tests will run against locally build modules,
# not against versions installed elsewhere.
#XXX: search for a better way to do that
import os
import os.path
import sys

THISDIR = os.path.dirname(__file__)
ROOTDIR = os.path.normpath(os.path.join(os.path.dirname(__file__), '../..'))
os.environ['LD_LIBRARY_PATH'] = os.path.join(ROOTDIR, 'build-debug')
sys.path.insert(0, os.path.join(ROOTDIR, 'build-debug/bindings/python'))

import mastermind_cache

logging_config = dict(
    version=1,

    formatters={
        'default': {
            'format': '%(asctime)s.%(msecs)03d %(levelname)1.1s %(filename)s:%(lineno)d: %(message)s',
            'datefmt': '%Y-%m-%dT%H:%M:%S',
            'converter': 'time.gtime',
        },
        'color_default': {
            '()' : 'tornado.log.LogFormatter',
            # tornado.log.LogFormatter does colors
            'format': '%(color)s%(asctime)s.%(msecs)03d %(levelname)1.1s %(filename)s:%(lineno)d:%(end_color)s %(message)s',
            'datefmt': '%Y-%m-%dT%H:%M:%S',
        },
    },

    handlers={
        'mastermind_cache_file': {
            'class': 'logging.handlers.WatchedFileHandler',
            'formatter': 'default',
            'filename': 'libmastermind.log',
        },
        'console': {
            'class': 'logging.StreamHandler',
            'formatter': 'color_default',
            'stream': 'ext://sys.stderr',
        },
    },

    #IMPORTANT: turning off disable_existing_loggers makes it
    # possible to avoid explicit listing of every existing logger
    # in the loggers section
    disable_existing_loggers=False,

    root={
        # 'handlers': ['defaultlog_file'],
        'handlers': ['console'],
    },

    loggers={
        'mastermind_cache': {
            'handlers': ['mastermind_cache_file'],
            # 'handlers': ['console'],
            # do not pass access messages beyond this logger
            'propagate': False,
        },
    },
)

import logging
import logging.config
logging.config.dictConfig(logging_config)

log = logging.getLogger('testing')

import time
import pytest

def create_mastermind_cache():
    obj = mastermind_cache.MastermindCache(
        auto_start=False,
        remotes='cloud01e.mdst.yandex.net', # cocaine-v12 endpoint
        # remotes='mmproxy01g.mdst.yandex.net', # cocaine-v11 endpoint
        cache_path=os.path.join(THISDIR, 'libmastermind.test.cache'),
    )

    log.debug('mastermind_cache: retrieving data...')

    # start cache engine and wait until it comes into usable state
    obj.start()
    while not obj.is_valid():
        log.debug('mastermind_cache: waiting...')
        time.sleep(0.3)

    log.debug('mastermind_cache: ready')

    return obj

def test_aaa():
    mc = create_mastermind_cache()

    with pytest.raises(mastermind_cache.NamespaceNotFoundError):
        mc.find_namespace_state(0)

    ns = mc.find_namespace_state(5)
    assert ns.name() == "default"
    print ns.name(), ns.couples().get_groups(5)
