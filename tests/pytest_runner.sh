#!/bin/bash

manager="$1/manager.sh"

$manager prepare 4
$manager start

python $1/bindings/python/test.py

$manager clear

