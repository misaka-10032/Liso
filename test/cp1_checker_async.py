#!/usr/bin/python

import threading
from socket import *
import sys
import random
import os
import time

valid = True

if len(sys.argv) < 6:
    sys.stderr.write('Usage: %s <ip> <port> <#jobs> <max #reqs per job> \
                     <max # bytes to write at a time> \n' % (sys.argv[0]))
    sys.exit(1)

serverHost = gethostbyname(sys.argv[1])
serverPort = int(sys.argv[2])
numJobs = int(sys.argv[3])
maxReqs = int(sys.argv[4])
numBytes = int(sys.argv[5])

lock = threading.Lock()

def job():
    #sid = random.randint(0, numSockets-1)
    #s = sockets[sid]
    with lock:
        s = socket(AF_INET, SOCK_STREAM)
        s.connect((serverHost, serverPort))
    with lock:
        numReqs = random.randint(1, maxReqs)
        for _ in xrange(numReqs):
            random_len = random.randrange(1, numBytes)
            random_string = os.urandom(random_len)
            s.send(random_string)
            data = s.recv(random_len)
            if data != random_string:
                valid = False
    with lock:
        s.close()

threads = []
for _ in xrange(numJobs):
    t = threading.Thread(target=job)
    threads.append(t)
    t.daemon = True
    t.start()

for t in threads:
    t.join()

if not valid:
    print 'Failed!'
    sys.exit(1)

print "Success!"
