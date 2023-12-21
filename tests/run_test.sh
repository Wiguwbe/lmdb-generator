#!/bin/bash

set -ex

rm -rf db

../ddl database -o .

gcc test_main.c db*.c -o test_main -llmdb

./test_main
