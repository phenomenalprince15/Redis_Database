#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>

using namespace std;

const size_t k_max_msg = 4096;

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

/*
We need buffers for reading/writing, since in Non-Blocking IO mode operations are defered.
In non-blocking mode, input/output (I/O) operations are not guaranteed to complete immediately.
Instead, they might be deferred, meaning the operation will only partially succeed or not succeed at all at that moment. 

The satte is used to decide what to do with the connection.
There are 2 states for ongoing connecitons:
Request and Response
*/
enum {
    STATE_REQ = 0, // request
    STATE_RES = 1, // response
    STATE_END = 2, // mark the connection for deletion
};

struct Conn {
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0;
    uint8_t rbuf[4+k_max_msg];
    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0; // to track how much data has already been sent
    uint8_t wbuf[4+k_max_msg];
};

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

static void state_req(Conn *conn);
static void state_res(Conn *conn);

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn) {
    if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0) {
        msg("accept() error");
        return -1;  // error
    }

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    // creating the struct Conn
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    if (!conn) {
        close(connfd);
        return -1;
    }
    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    return 0;
}

// static int32_t one_request(int connfd) {
//     // 4 bytes header
//     char rbuf[4 + k_max_msg + 1];
//     errno = 0;
//     int32_t err = read_full(connfd, rbuf, 4);
//     if (err) {
//         if (errno == 0) {
//             msg("EOF");
//         } else {
//             msg("read() error");
//         }
//         return err;
//     }

//     uint32_t len = 0; // extract the len of message from 4 bytes
//     memcpy(&len, rbuf, 4);  // assume little endian
//     if (len > k_max_msg) {
//         msg("too long");
//         return -1;
//     }

//     // request body
//     err = read_full(connfd, &rbuf[4], len); // Read the message body from the client. The body starts at rbuf[4] and has a length of len.
//     if (err) {
//         msg("read() error");
//         return err;
//     }

//     // do something
//     rbuf[4 + len] = '\0';
//     printf("client says: %s\n", &rbuf[4]);

//     // reply using the same protocol (Prepare and send a reply)
//     const char reply[] = "world";
//     char wbuf[4 + sizeof(reply)];
//     len = (uint32_t)strlen(reply);
//     memcpy(wbuf, &len, 4);
//     memcpy(&wbuf[4], reply, len);
//     return write_all(connfd, wbuf, 4 + len);
// }

static bool try_one_request(Conn *conn) {
    // try to parse a request from the buffer
    if (conn->rbuf_size < 4) {
        // not enough data in the buffer
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4); 
    if (len > k_max_msg) {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }

    if (4 + len > conn->rbuf_size) {
        // not enough data in the buffer
        return false;
    }

    // got one request, do something with it
    printf("Client says: %.*s\n", len, &conn->rbuf[4]);

    // generating echoing response
    memcpy(&conn->wbuf[0], &len, 4);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], len);
    conn->wbuf_size = 4 + len;

    // remove the request from the buffer.
    // note: frequent memmove is inefficient.
    // note: need better handling for production code.
    size_t remain = conn->rbuf_size - 4 - len;
    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    // change state
    conn->state = STATE_RES;
    state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

/**
 * @brief Attempt to read data from the connection's file descriptor into the read buffer.
 *
 * This function performs non-blocking I/O operations to fill the read buffer (`rbuf`) associated 
 * with a connection. It ensures that the buffer is not overfilled and gracefully handles various 
 * edge cases such as interrupted system calls (EINTR) and temporary unavailability of data (EAGAIN).
 * 
 * The function also checks for EOF conditions and updates the connection's state accordingly.
 * If the buffer contains enough data for one or more complete requests, it processes each request 
 * sequentially (pipelining) by calling `try_one_request()` in a loop. The connection state transitions
 * based on the successful processing of data (from `STATE_REQ` to `STATE_RES` or `STATE_END`).
 *
 * @param conn Pointer to the connection structure containing the state and buffers.
 * @return true if the connection is still in the `STATE_REQ` state and ready for further reading.
 * @return false if the connection has encountered an error, reached EOF, or moved to another state.
 */
static bool try_fill_buffer(Conn *conn) {
    // try to fill the buffer
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;
    do{
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size; // remaining capacity in buffer
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap); // reading data from current buffer position
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop
        return false;
    }

    if (rv < 0) {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0) { // Connection is closed (EOF)
        if (conn->rbuf_size > 0) {
            msg("Unexpected EOF");
        } else {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    // if read succeeds, conn->rbuf_size is updated by number of bytes read
    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf)); // to make sure new buffer size doesn't exceed the total buffer capacity

    // Try to process requests one by one
    // Why is there a loop ? "Pipelining", handling multiple requests from client in single read
    while (try_one_request(conn)) {}
    return (conn->state == STATE_REQ);

}

/**
 * @brief Handle the request processing state (`STATE_REQ`) for a connection.
 *
 * This function is responsible for managing the connection when it is in the request-handling phase.
 * It continuously attempts to fill the read buffer by calling `try_fill_buffer()` until the buffer
 * can no longer be filled or the connection transitions to another state (e.g., `STATE_RES` or `STATE_END`).
 *
 * @param conn Pointer to the connection structure containing the state and buffers.
 */
static void state_req(Conn *conn) {
    while (try_fill_buffer(conn)) {} // Keep filling the buffer as long as there is room and data
}

static bool try_flush_buffer(Conn *conn) {
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0) {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }

    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);

    if (conn->wbuf_sent == conn->wbuf_size) {
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }

    // still got some data in wbuf, could try to write again
    return true;
}

static void state_res(Conn *conn) {
    while (try_flush_buffer(conn)) {}
}

static void connection_io(Conn *conn) {
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0); // not expected
    }
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // this is needed for most server applications
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    vector<Conn*> fd2conn;
    
    // set the listen fd to non-blocking mode
    fd_set_nb(fd);

    // the event loop
    vector<pollfd> poll_args;
    
    while (true) {
        // prepare the arguments of the poll()
        poll_args.clear();

        // for convenience, the listening fd is put in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        // connection fds
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            if (conn->state == STATE_REQ) {
                pfd.events = POLLIN;
            } else {
                pfd.events = POLLOUT;
            }
            // pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        // poll for active fds
        // the timeout argument doesn't matter here
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }

        // process active connections
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents) {
                Conn *conn = fd2conn[poll_args[i].fd];
                connection_io(conn);
                if (conn->state == STATE_END) {
                    // client closed normally, or something bad happened.
                    // destroy this connection
                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        // try to accept a new connection if the listening fd is active
        if (poll_args[0].revents) {
            (void)accept_new_conn(fd2conn, fd);
        }

    }


    // while (true) {
    //     // accept
    //     struct sockaddr_in client_addr = {};
    //     socklen_t socklen = sizeof(client_addr);
    //     int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    //     if (connfd < 0) {
    //         continue;   // error
    //     }

    //     while (true) {
    //         // here the server only serves one client connection at once
    //         int32_t err = one_request(connfd);
    //         if (err) {
    //             break;
    //         }
    //     }
    //     close(connfd);
    // }

    return 0;
}