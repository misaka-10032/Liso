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

* To test pipelined request, I `telnet` to the server, and send multiple requests, including good ones and malformed ones.
