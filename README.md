# Inter-Process Communication (Linux)

## Methods
- [x] *File*              - open/write/read
- [x] *Signal*            - signal/kill
- [ ] *Socket*            - AF_INET/UNIX, SOCK_STREAM/DGRAM
- [ ] *Message Queue*     - mq_overview
- [ ] *Pipe*              - pipe/mkfifo
- [x] *Shared Memory*     - shm_open/mmap

## Shared Memory vs Unix Socket Benchmark
To build:
```bash
mkdir build && cd build
cmake ..
make
```

To build SPSC mmap version (implemented with atomics, not optimized):
```bash
mkdir build && cd build
cmake .. -DUSE_SPSC_MMAP=1
make
```

To run (from the project root):
```bash
./benchmark.sh
```

My results:
```
$ ./benchmark.sh
        msg size        mmap            socket
            1           213.751 ms      405.351 ms
            8           209.754 ms      387.723 ms
           16           216.695 ms      376.326 ms
           64           263.746 ms      379.447 ms
          128           431.233 ms      392.534 ms
          256           497.671 ms      415.736 ms
          512           823.227 ms      418.491 ms
         1024           1606.08 ms      435.693 ms
         2048           1476.56 ms      494.805 ms
         4096           1959.35 ms      748.82 ms
        16384           5996.13 ms      2105.48 ms
```

With `-DUSE_SPSC_MMAP=1`
```
        msg size        mmap            socket
            1           86.7701 ms      375.395 ms
            8           86.8216 ms      354.691 ms
           16           95.6893 ms      344.511 ms
           64           96.3285 ms      354.857 ms
          128           115.218 ms      359.521 ms
          256           143.884 ms      399.863 ms
          512           251.388 ms      383.167 ms
         1024           435.271 ms      422.819 ms
         2048           491.43 ms       486.259 ms
         4096           744.26 ms       783.066 ms
        16384           1875.53 ms      2027.99 ms
