# HTTP Threadripper

HTTP 1.1 server which shows off high concurrency capabilities of C++.

Implemented using no third-party http libraries.
Uses [libev](https://github.com/enki/libev) and unix sockets.

### To compile

```bash
g++ -O3 server.cpp -lev -o server
```

### To run

```bash
./server 8000
```

### To run benchmarks

```bash
httperf --server localhost --port 8000 --num-conns 10000 --rate 100000
```

## Notes
 - This is just a hello world server.


## Sample benchmark

The program achieves 65232.4 req/s on one thread and 20 concurrent connections.
I was not able to benchmark 10,000 concurrent users but I expect similar results

```
$> httperf --hog --server localhost --port 8000 --num-conns 20 --rate 100000 --num-calls 5000

httperf --hog --client=0/1 --server=localhost --port=8000 --uri=/ --rate=100000 --send-buffer=4096 --recv-buffer=16384 --num-conns=20 --num-calls=5000
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
Maximum connect burst length: 1

Total: connections 20 requests 100000 replies 100000 test-duration 1.533 s

Connection rate: 13.0 conn/s (76.6 ms/conn, <=20 concurrent connections)
Connection time [ms]: min 1409.1 avg 1491.3 max 1531.1 median 1500.5 stddev 33.4
Connection time [ms]: connect 4.2
Connection length [replies/conn]: 5000.000

Request rate: 65232.4 req/s (0.0 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 0.0 avg 0.0 max 0.0 stddev 0.0 (0 samples)
Reply time [ms]: response 0.3 transfer 0.0
Reply size [B]: header 62.0 content 12.0 footer 0.0 (total 74.0)
Reply status: 1xx=0 2xx=100000 3xx=0 4xx=0 5xx=0

CPU time [s]: user 0.22 system 1.28 (user 14.1% system 83.6% total 97.7%)
Net I/O: 8663.7 KB/s (71.0*10^6 bps)

Errors: total 0 client-timo 0 socket-timo 0 connrefused 0 connreset 0
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
```
