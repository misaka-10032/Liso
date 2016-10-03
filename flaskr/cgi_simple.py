#!/usr/bin/env python
#
#   This script executes a minimal CGI test function.  Output should be valid
#   HTML and a valid response to the user.  It is an example of a script
#   conforming to RFC 3875 Section: 5.  NPH Scripts.
#
#   Authors: Athula Balachandran <abalacha@cs.cmu.edu>
#            Charles Rang <rang@cs.cmu.edu>
#            Wolfgang Richter <wolf@cs.cmu.edu>

from os import environ
import sys
import cgi

#import cgitb
#cgitb.enable()

with open('tmp', 'w') as f:
    f.write('cgi begins.\n')

print 'HTTP/1.1 200 OK\r\n',
print 'Server: %s\r\n' % (environ['SERVER_SOFTWARE']),

s = sys.stdin.read()
with open('tmp', 'a') as f:
    f.write('body is %s\n' % s)

print '[cgi] read %s' % s
print 'hehe'

#cgi.test()
