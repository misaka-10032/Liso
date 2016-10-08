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
import cgi
import cgitb
import sys
import cStringIO

cgitb.enable()

buf = cStringIO.StringIO()
old_stdout = sys.stdout
sys.stdout = buf
cgi.test()
sys.stdout = old_stdout
out = buf.getvalue()

print 'HTTP/1.1 200 OK\r\n',
length = len(out) - len('Content-type: text/html') - 10
print 'Content-Length: %d\r\n' % length,
print 'Server: %s\r\n' % (environ['SERVER_SOFTWARE']),

print out

