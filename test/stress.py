#!/usr/bin/python

import threading
from socket import *
import sys
import random
import os
import time
import requests
import random

from requests.packages.urllib3.exceptions \
        import InsecureRequestWarning, SubjectAltNameWarning
requests.packages.urllib3.disable_warnings(InsecureRequestWarning)
requests.packages.urllib3.disable_warnings(SubjectAltNameWarning)

if len(sys.argv) < 5:
    sys.stderr.write('Usage: %s <host> <http_port> <https_port> <cert>' \
                     % (sys.argv[0]))
    sys.exit(1)

host = sys.argv[1]
http_port = int(sys.argv[2])
https_port = int(sys.argv[3])
cert = sys.argv[4]

num_jobs = 1000
iters = 1000

goods = [(requests.get, 'http://{}:{}'.format(host, http_port), None),
         (requests.get, 'https://{}:{}'.format(host, https_port), None),
         (requests.head, 'http://{}:{}'.format(host, https_port), None),
         (requests.head, 'https://{}:{}'.format(host, https_port), None),
         ]

bads  = [(requests.get, 'http://{}:{}/gg'.format(host, http_port), None),
         (requests.get, 'https://{}:{}/gg'.format(host, https_port), None),
         (requests.head, 'http://{}:{}/gg'.format(host, https_port), None),
         (requests.head, 'https://{}:{}/gg'.format(host, https_port), None),
         ]

#lock = threading.Lock()

valid = True


def job():
    for _ in xrange(iters):
        #with lock:
        #    pass

        """ good """
        func, url, data = goods[threading.current_thread().ident % len(goods)]

        resp = func(url, verify=cert, data=data)
        if resp.status_code != 200:
            print resp.status_code, func.__name__, url
            valid = False

        """ bad """
        func, url, data = bads[threading.current_thread().ident % len(goods)]

        resp = func(url, verify=cert, data=data)
        if resp.status_code == 200:
            print resp.status_code, func.__name__, url
            valid = False


threads = []
for _ in xrange(num_jobs):
    t = threading.Thread(target=job)
    threads.append(t)
    #t.daemon = True
    t.start()

for t in threads:
    t.join()

if valid:
    print "Success!"
else:
    sys.stderr.write("Failed!\n")
    sys.exit(1)

