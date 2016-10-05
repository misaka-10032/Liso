#!/usr/bin/env python
#
# This script serves as a simple TLSv1 client for 15-441
#
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,
#          Charles Rang <rang972@gmail.com>,
#          Wolfgang Richter <wolf@cs.cmu.edu>

import pprint
import socket
import ssl

port = 10443

req = "GET /cgi/login HTTP/1.1\r\n" \
      "Host: localhost\r\n" \
      "Connection: close\r\n" \
      "Accept: text/html\r\n" \
      "User-Agent: Mozilla/5.0\r\n" \
      "Accept-Encoding: gzip,deflate,sdch\r\n" \
      "Accept-Language: en-US,en;q=0.8\r\n" \
      "\r\n"

# try a connection
sock = socket.create_connection(('localhost', port))
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='signer.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)

# what cert did he present?
pprint.pprint(tls.getpeercert())

tls.sendall(req)
print tls.recv(4096)
print tls.recv(4096)
tls.close()

exit(0)
