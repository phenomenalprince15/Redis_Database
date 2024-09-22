#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        fprintf(stderr,"read()");
        return;
    }

    printf("Client says: %s\n", rbuf);

    char wbuf[] = "Hey there, you have reached the server!\n";
    write(connfd, wbuf, strlen(wbuf));

}

int main() {
    // create socket fd
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // server is different here, becz it needs to host
    // server will set the socket options
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // server address details
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    // Bind address to the socket, like we had connect() for client
    // we have bind for server
    int rv = bind(fd, (const sockaddr *) &addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // listen to incoming connection, listen()
    rv = listen(fd, SOMAXCONN);
    if(rv) {
        die("listen()");
    }

    // Accept the connection and handle client connections
    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *) &client_addr, &socklen);
        if (connfd < 0) {
            continue;
        }

        do_something(connfd);
        close(connfd);
    }


    return (0);
}