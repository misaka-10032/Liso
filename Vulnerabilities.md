# Vulnerabilities

### Checkpoint 1

* `logging` module is not thread-safe, nor process-safe. I don't support it because I don't have to, as Liso is a single-threaded select based server.
* I don't support time-to-live (TTL) yet. If more than `FD_SIZE` clients connect to the server and don't close, the server would DOS to the new requests.

### Checkpoint 2

* The previous two vulnerabilities still exist.

### Checkpoint 3

* [Solved] Use `lockf` before and after logging.
* TTL is still not supported yet.
