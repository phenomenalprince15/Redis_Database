#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for read(), write(), and close()

/*
Typical workflow on the server side

fd = socket()         // Create a socket for listening for connections
bind(fd, address)     // Bind the socket to a specific address (port and IP)
listen(fd)            // Start listening for incoming connections
while (true) {
    conn_fd = accept(fd)      // Accept an incoming connection
    do_something_with(conn_fd) // Process the connection (read/write data)
    close(conn_fd)            // Close the connection once processing is done
}

socket() - returns a file descriptor (fd) that can be used for communication
bind()   - associates an address (IP and port) with the socket fd
listen() - allows the server to accept incoming connections to that address
accept() - waits for an incoming client connection and returns a new connection fd
read()   - receives data from the client through the connection
write()  - sends data to the client
close()  - closes the socket when done with communication
*/

// Prints a message to the standard error stream
static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

// Prints an error message and terminates the program
static void die(const char *msg) {
    int err = errno; // Fetch the current error code (if any)
    fprintf(stderr, "[%d] %s\n", err, msg); // Print the error code and message
    abort(); // Terminate the program
}

// Handles communication with the client on the connection fd
static void do_something(int connfd) {
    char rbuf[64] = {}; // Buffer to receive data from the client
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1); // Read from the client
    if (n < 0) { // If the read fails
        msg("read() error");
        return;
    }

    printf("Client says: %s\n", rbuf); // Print the message received from the client
    char wbuf[] = "world"; // Prepare a response message
    write(connfd, wbuf, strlen(wbuf)); // Write the response to the client
}

int main(int argc, char *argv[]) {
    // Step 1: Create a socket for the server
    int fd = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
    if (fd < 0) { // If socket creation fails
        die("socket()"); // Handle the error
    }    

    // Step 2: Set socket options (allow reuse of the address)
    int val = 1; // Enable the option
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)); // Allow reuse of address

    // Step 3: Bind the socket to an IP address and port
    struct sockaddr_in addr = {}; // Define the server address structure
    addr.sin_family = AF_INET;  // Use IPv4 address family
    addr.sin_port = htons(1234); // Set the port to 1234 (convert to network byte order with htons)
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all available network interfaces (0.0.0.0)

    // Bind the address to the socket
    int rv = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (rv) { // If binding fails
        die("bind()"); // Handle the error
    }

    // Step 4: Start listening for incoming connections
    rv = listen(fd, SOMAXCONN); // Listen with a maximum backlog of connections (OS defined limit)
    if (rv) { // If listening fails
        die("listen()"); // Handle the error
    }

    // Step 5: Accept and handle client connections in a loop
    while (true) {
        struct sockaddr_in client_addr = {}; // Client address structure
        socklen_t socklen = sizeof(client_addr); // Size of client address structure
        int connfd = accept(fd, (struct sockaddr *) &client_addr, &socklen); // Wait for a connection
        if (connfd < 0) { // If accept fails
            continue; // Ignore the error and continue accepting
        }

        // Step 6: Process the client connection
        do_something(connfd); // Read from and write to the client
        close(connfd); // Close the connection once done
    }

    return 0;
}
