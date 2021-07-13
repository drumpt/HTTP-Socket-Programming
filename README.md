# HTTP Socket Programming
## :scroll: Description
- Simple HTTP 1.0 server and client implementation via TCP socket programming in C
- Client : supports GET and POST requests of HTTP 1.0
- Server : supports only the following RESPONSE status codes (200 OK / 400 Bad Request / 404 Not Found)
- Implemented Headers : Host, Content-Length, Content-Type

## :computer: Usage
### Build
```bash
$ make all
```

### Client
#### GET
```bash
./client -G URL
```

#### POST
```bash
./client -P URL
```

### Server
```bash
./server -p PORT_NUMBER
```

### Test
```bash
$ make test TESTFILE=file_4K.bin
```

### Test All
```bash
$ make test-all
```

## :cactus: Testing Environment
### Compiler
```
gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0
```

### LibC
```
GNU C Library (Ubuntu GLIBC 2.31-0ubuntu9.2) stable release version 2.31.
```

### Make
```
GNU Make 4.2.1
```

### Linux Kernel
```
Linux 4.19.0-14-amd64 #1 SMP Debian 4.19.171-2 (2021-01-30) x86_64 GNU/Linux
```
