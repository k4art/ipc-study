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

To run (from the project root):
```bash
./benchmark.sh
```

My results:
```
$ ./benchmark.sh
       msg size        mmap            socket
           1           141.117 ms      386.335 ms
          16           139.421 ms      384.627 ms
          64           198.201 ms      388.212 ms
         512           829.918 ms      412.536 ms
        2048           1414.97 ms      506.816 ms
        4096           1891.71 ms      857.139 ms
       16384           6200.99 ms      2033.1 ms
```
