This is a simple Redis-like In-Memory Database that supports point queries, range queries and TTL (time-to-live) operations.

How to Build & Run:
Build: make
Run the server: ./main
Connect to server: ./client <command>

Supported commands:
Point queries (HashMap - based):
1. get <key> - retrieve a value for key, O(1) on average
2. set <key> <value> - set a value for key, O(1) on average
3. del <key> - remove key from the DB, O(1) on average

TTL:
1. expire <key> <ttl> - set a timeout (ttl) for key, O(logN) on average
2. ttl <key> - get the remaining ttl for key, O(1) on average
3. persist <key> - remove the ttl to turn key to persistent, O(logN) on average

Range queries (Sorted Set - based):
1. zadd <key> <value> - add (key, value) to the sorted set, O(logN) on average
2. zrem <key> - remove (key, value) from the sorted set, O(logN) on average
3. zrange <from> <offset> - get a list of at most <offset> keys starting <from>, O(logN + offset) on average

Performance - Oriented Features:
1. Event Loop & Non-Blocking Sockets:
   The server handles concurrency using an event loop based on poll().
   All sockets are configured as non-blocking, preventing blocking on I/O operations and avoiding the overhead of thread context switching.

2. Gradual Rehashing:
   A custom hashmap supports gradual rehashing, spreading the expenses over time to avoid latency during table resizing.
   
3. Efficient timeouted keys handling:
   TTLManager uses a min-heap to track expiring keys, enabling efficient removal in O(number of expired keys).
   Cleanup is performed gradually to avoid performance issues when many keys expire simultaneously.
   
4. RingBuffers for I/O:
   Both input and output buffers are implemented as ring buffers to prevent latency during buffer resizing.



Inspired by core Redis concepts, but written from scratch for learning purposes.
