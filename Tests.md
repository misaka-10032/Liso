# Tests

### Checkpoint 1

To test an echo server

```
git checkout checkpoint-1
make test1
```

* Inside the automation, I first do synchronized send and recv by using the provided checker.

```
cp1_checker.py  ...
```

* Then, I refactored checker to send and recv asynchronously in multiple threads.

```
cp1_checker_async.py  ...
```

### Checkpoint 2

To test a static page server

```
git checkout checkpoint-2
make test2
```

* Parser is based on some of the utility functions, so extra test on these utilities are tested

```
./test_driver
```

* To test streamed `send`, I tried to send the request char by char.

```
./test/send_one_by_one.py
```

* To test pipelined request, I `telnet` to the server, and send multiple requests, including good ones and malformed ones. Toggle `CRLF` to send `\r\n` on enter.

```
telnet> toggle CRLF
```

### Checkpoint 3

* When setting up CGI, have CGI print everything in log file to make sure it receives everything it needs.

### Usability test

* To test static pages, simply use multiple browsers to view it.
* To test CGI, try with the `flaskr` blog. Go through login, post blog, logout.
* To test HTTPS, import _CMU CA_ to browser, and visit the sites via HTTPS port. Both static and dynamic sites are tested under HTTPS.
* `telnet` is used to test some edge cases, such as multiple requests sent at a time.

```
HEAD /index.html HTTP/1.1
Host: 127.0.0.1:10032
Connection: Keep-Alive

HEAD /index.html HTTP/1.1
Host: 127.0.0.1:10032
Connection: Keep-Alive

HEAD /index.html HTTP/1.1
Host: 127.0.0.1:10032
Connection: Close
```

### Memory check

* Valgrind check

```
valgrind --leak-check=full --trace-children=yes \
         --log-file=run/valgrind.log ...
```

The summary is like

```
==99647== LEAK SUMMARY:                                                             
==99647==    definitely lost: 0 bytes in 0 blocks                                   
==99647==    indirectly lost: 0 bytes in 0 blocks                                   
==99647==    possibly lost: 2,064 bytes in 1 blocks                               
==99647==    still reachable: 8,664 bytes in 2 blocks                               
==99647==    suppressed: 20,707 bytes in 194 blocks
```

Shortcoming is that it refuses to `execve` CGI scripts.

* Pool size check. I would manually log the connection pool size (`poolsz`), every time when it changes. I opened several browsers to view the websites, and then quite all the browsers. I would make sure `poolsz` drops to 0.

### Siege test

```
make siege
```

### Stress test

```
make stress
```

* This will trigger bunch of asynchronous requests to the server. In the meanwhile, I will view the webpages via browser. Make sure it still shows smoothly.