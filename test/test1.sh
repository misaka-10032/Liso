#!/bin/bash

dir=$( dirname "$BASH_SOURCE[0]" )

echo 'Checking sync send/recv...'
python $dir/cp1_checker.py localhost 10032 20 100 8000 100
if [ $? != "0" ]; then
    echo "cp1 sync failed."
    exit 1
fi

echo 'Checking async send/recv...'
# <host> <port> <#jobs> <#req/job> <#bytes>
python $dir/cp1_checker_async.py localhost 10032 100 100 8000
if [ $? != "0" ]; then
    echo "cp1 async failed."
    exit 1
fi

echo "Success!"
