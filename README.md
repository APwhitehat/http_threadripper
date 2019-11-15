To compile

g++ -O3 server.cpp -lev -o server

To run

./server 8000

To run benchmarks

httperf --server localhost --port 8000 --num-conns 10000 --rate 100000

Notes
 - This is just a hello world server.

