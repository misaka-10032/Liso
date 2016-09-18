#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

import resource
resource.setrlimit(resource.RLIMIT_NOFILE, (3000, 3000))

to_send = '''GET / HTTP/1.1\r
Host: localhost\r
Connection: close\r
Accept: text/html\r
User-Agent:   Mozilla/5.0\r
Accept-Encoding:   gzip,  deflate,  sdch\r
Accept-Language:   en-US, en;  q=0.8\r
\r\n
'''

if len(sys.argv) < 2:
    sys.stderr.write('Usage: %s <ip> <port>\n' % (sys.argv[0]))
    sys.exit(1)

serverHost = gethostbyname(sys.argv[1])
serverPort = int(sys.argv[2])

s = socket(AF_INET, SOCK_STREAM)
s.connect((serverHost, serverPort))

for i in xrange(len(to_send)):
    s.send(to_send[i])

print s.recv(9999999)
print s.recv(9999999)
s.close()

