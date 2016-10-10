# Liso

## Features

* HTTP/1.1: GET, HEAD, POST.
* HTTPS via TSL
* CGI

```
./lisod <http_port> <https_port> <log_file> \
        <lock_file> <www_folder> <cgi_path> \
        <private_key_file> <certificate_file>
```

## Code Overview

* `lisod`: the Liso server.
* `client`: an echo client for testing.
* `pool`: connection pool managing accept/drop/reset connections.
* `conn`: connection object handling send/recv data.
* `buffer`: buffering to adapt send/recv rates.
* `header`: http headers organized in singly linked list.
* `request`: structured request, along with parser.
* `response`: structured response, along with builder.
* `logging`: the logging module.
* `utils`: utility functions.
* `test_driver`: unit test for utility functions.

### Connection

Connection object is responsible to serve the client. It holds `request`, `response`, and a common `buffer` to send/recv data. It provides callbacks to hook up to the main server.

* `SuccCb` is success callback. If any state needs to updated after we successfully recv/send some data, this function will be called.
* `ErrCb` is error callback. It will be called when recoverable error is encountered, e.g. when we meet malformed request.
* `FatCb` is fatality callback. It will be called when fatal error is encountered, e.g. when client is lost. At this time, server should clean up the connection.

The difference between `ErrCb` and `FatCb` is that `ErrCb` is responsible for recoverable errors, and `FatCb` is responsible for the fatal ones. Server should try to response error message to client in `ErrCb`, and clean up connection resource in `FatCb`.

### Pool

Pool is designed to handle add/delete/reset of connections. It also manages `read_set` and `write_set` for `select`, because every change of connection state would lead to the update in these two sets. The connections are arranged as an array. Each connection knows its index in the array. Once fatal error occurs, we replace it with the last connection in the pool, and then delete it.

### CGI

* CGI model from [RFC 3050](https://www.ietf.org/rfc/rfc3050.txt).

```
                -----    ------------
     ~~~~~~~~  |req  |  |  --------  |
    |        |----------| |  http  | |
    | client | |resp |  | | server | |
    |        |----------| |        | |w
     ~~~~~~~~  |     |  |  --------  |e
                -----   |  s|  /\s   |b
               net      |  t|   |t   |
                        |e d| C |d   |s
                        |n i| G |o   |e
                        |v n| I |u   |r
                        |   |   |t   |v
                        |  \/   |    |e
                        |  -------   |r
                        | |       |  |
                        | |  CGI  |  |
                        | | prog. |  |
                        | |       |  |
                        |  -------   |
                         ------------

```

Three pipes (for stdin, stdout, stderr) are created between server and CGI program. Server simply communicate via pipe. CGI would `dup` its stdin, stdout, stderr to the pipe.

Server adds `stdout_pipe` and `stderr_pipe` into `read_set` for select. Once server receives content from `stdout_pipe` from CGI, it stores the content in buffer, and prepare to send to client. Once server receives content from `stderr_pipe` from CGI, it simply throws the error message to `logging` module to log it down as error.
