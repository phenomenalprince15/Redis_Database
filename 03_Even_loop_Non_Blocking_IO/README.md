# Server and Client Event Loop

## Understanding Client Side
- Create a socket (which gives a fd)
- Fill in details of that socket with address, port, etc...
- Connect, using connect() - > pass fd and other params (Try connecting with server)
- Send queries (multiple as well, create a list) it's called pipelined requests
- For each requests - send_req(fd, query_list[0...n-1]) // do something and Handle the errors
- For each query after request - read_res() // read response from server if sent by server (Great!), Handle errors

### How to send_req() and how to read_res() (Request and Response)
#### send_req()
- How do you send a request to server ??? What do you need ???
- A fd and text to send
- Prepare that text and write in buffer to send (think of headers also, here we have len=4 as header for length of text and then we store the text, taking care of that, write in buffer from [0 to 3, 4 to max_len])
- Use memcpy to copy len of header, store 4 bytes of data in [0 to 3]
memcpy(wbuf, &len, 4)
- Use memcpy again to copy the text of len bytes
memcpy(&wbuf[4], text, len)
- write()

#### read_res()
 - How to you read response ??? What do you need ???
 - Ofc a fd, now you need to read a response what you got from the server
 - you need read buffer to read, rbuf[4 + max_len + 1] // 4 for header len, max_len for message + 1 for terminating null character
 - just use read() to read first 4 bytes which gives us len of text, handle the errors
 - Extract the reply length and read the reply, but how ?
 - Use memcpy, memcpy(&len, rbuf, 4) // rbuf is the source which will copy the first 4 bytes to len, handle the errors
 - now we can read how much text is of length len, read(fd, &rbuf[4], len) // read from [4 to len]
- rbuf[4 + len] = '\0' terminate after reading and printf()

## Understanding Server Side
- Create a socket (which gives a fd)
- Set the appropriate address, port etc...
- Bind the address, bind() -> pass fd and addr and sizeof addr ofc that needs binding
- Thens start listening to incoming connections of client, listen(fd, SOMAXCONN)
- create a map of all client connections, keyed to fd, list of connections with fd (vector<Conn*> fd2Conn), Conn is a struct
- set the listen fds to non-blocking mode, fd_set_nb(fd)
- Then the event loop which requires system calls, pollfd, poll
- Follow the general code below for our server to handle requests

```
all_fds = [...]
while True:
    active_fds = poll(all_fds)
    
    for each fd in active_fds:
        do_something_with(fd)
        
def do_something_with(fd):
    if fd is a listening socket:
        add_new_client(fd)
    elif fd is a client connection:
        while work_not_done(fd):
        do_something_to_client(fd)

def do_something_to_client(fd):
    if should_read_from(fd):
        data = read_until_EAGAIN(fd)
        process_incoming_data(data)
    while should_write_to(fd):
        write_until_EAGAIN(fd)
    if should_close(fd):
        destroy_client(fd)
```

## Let's understand memcpy, memset, memmove

1. memcpy
- Prototype: void *memcpy(void *dest, const void *src, size_t n);
- Purpose: Copies n bytes from the memory area pointed to by src to the memory area pointed to by dest.
- Usage: Used to copy a fixed number of bytes from one location to another.
- Limitation: The source and destination memory regions must not overlap. If they overlap, the behavior is undefined.
```
int src[] = {1, 2, 3, 4};
int dest[4];
memcpy(dest, src, sizeof(src));  // Copies entire content of src into dest
```

2. memmove
- Prototype: void *memmove(void *dest, const void *src, size_t n);
- Purpose: Similar to memcpy, but can handle overlapping memory regions safely. It ensures that the original data in src is copied to dest without corruption, even when they overlap.
- Usage: Used when the source and destination areas might overlap
```
char str[] = "Hello, World!";
memmove(str + 7, str, 5);  // Safely moves "Hello" to overlap with "World"
```

3. memset
- Prototype: void *memset(void *ptr, int value, size_t n);
- Purpose: Fills the first n bytes of the memory area pointed to by ptr with the specified value (converted to an unsigned char).
- Usage: Commonly used to initialize or reset a block of memory to a specific value, often zero.
```
char buffer[10];
memset(buffer, 0, sizeof(buffer));  // Sets all bytes in buffer to 0
```

4. memcmp
- Prototype: int memcmp(const void *ptr1, const void *ptr2, size_t n);
- Purpose: Compares the first n bytes of the memory areas ptr1 and ptr2. Returns 0 if they are equal, a value less than 0 if ptr1 is less than ptr2, and a value greater than 0 if ptr1 is greater than ptr2.
- Usage: Used to compare raw memory blocks byte by byte
```
char buffer1[] = "ABC";
char buffer2[] = "ABD";
int result = memcmp(buffer1, buffer2, 3);  // result will be negative because 'C' < 'D'
```


## System calls used

## Usage of `pollfd()`
`pollfd` is a structure used with the `poll()` system call in C for monitoring multiple file descriptors to see if I/O is possible on any of them. It is commonly utilized in network programming for handling non-blocking I/O.

## Structure Definition
```c
struct pollfd {
    int fd;         // File descriptor to be monitored
    short events;   // Events to look for (input/output events)
    short revents;  // Events that occurred
    // Events of Interest
    //   POLLIN: Data available for reading.
    //   POLLOUT: Ready for writing.
    //   POLLERR: Error condition.
    //   POLLHUP: Connection hung up.
};
```

## Usage of `poll()`
The `poll()` function waits for events on the file descriptors specified in an array of `pollfd` structures. It allows you to monitor multiple file descriptors to see if they are ready for reading or writing, enabling non-blocking I/O operations.

### Syntax
```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
// Parameters for poll()
struct pollfd *fds;     // A pointer to an array of pollfd structures that describe the file descriptors to be monitored.
nfds_t nfds;            // The number of file descriptors in the fds array.
int timeout;            // Maximum time to wait for an event (in milliseconds):
                        //   - Use -1 for indefinite wait (blocking).
                        //   - Use 0 for immediate return (non-blocking).
                        //   - Use a positive value for a specific timeout period.
```

## Event Loop Visualization
```
+----------------------+
|     Event Loop       |
|                      |
|  1. Clear poll_args  |
|                      |
|  2. Add listening fd  |
|                      |
|  3. Add client fds   |
|                      |
+----------------------+
          |
          v
+----------------------+
|     Call poll()      |
|                      |
|  4. Wait for events  |
|                      |
+----------------------+
          |
          v
+----------------------+
| Process Active FDs   |
|                      |
|  5. Handle I/O for   |
|     active clients    |
|                      |
|  6. Cleanup closed    |
|     connections       |
+----------------------+
          |
          v
+----------------------+
| Accept New Connection |
|                      |
|  7. Check listening fd|
|                      |
+----------------------+

```
