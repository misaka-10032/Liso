# Vulnerabilities

* `logging` module is not thread-safe. I don't support it because I don't have to, as Liso is a single-threaded select based server.
* I don't support time-to-live yet. If more than `FD_SIZE` clients connect to the server and don't close, the server would DOS to the new requests.