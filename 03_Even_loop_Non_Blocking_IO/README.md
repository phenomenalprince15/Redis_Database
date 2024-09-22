# System calls used

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
