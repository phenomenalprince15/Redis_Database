#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for read(), write(), and close()

/*
Typical workflow in client

fd = socket()         // Create a socket for communication
connect(fd, address)  // Connect the socket to a specific address (server)
do_something_with(fd) // Use the socket for communication (e.g., send and receive data)
close(fd)             // Close the socket when done
*/

// Function to handle errors and terminate the program
static void die(const char *msg) {
    int err = errno; // Fetch the current error code (if any)
    fprintf(stderr, "[%d] %s\n", err, msg); // Print the error message along with the error code
    abort(); // Terminate the program
}

int main(int argc, char *argv[]) {

    // Step 1: Create a socket for the client
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { // If socket creation fails
        die("socket()"); // Handle the error using the die() function
    }

    // Step 2: Specify the server address details
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; // Use IPv4 address family
    addr.sin_port = ntohs(1234); // Set the port to 1234 (ensure network byte order with ntohs)
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // Use loopback address (127.0.0.1) for localhost

    // Step 3: Connect to the server
    int rv = connect(fd, (const struct sockaddr *) &addr, sizeof(addr));
    if (rv) { // If connection fails
        die("connect"); // Handle the error
    }

    // Step 4: Send a message to the server
    char msg[] = "Hello";
    write(fd, msg, strlen(msg)); // Write the message to the socket

    // Step 5: Read the server's response
    char rbuf[64] = {}; // Buffer to store the server's response
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1); // Read the response
    if (n < 0) { // If reading fails
        die("read"); // Handle the error
    }

    // Step 6: Print the server's response
    printf("Server says: %s\n", rbuf); // Display the message received from the server

    // Step 7: Close the socket connection
    close(fd); // Close the socket to free resources

    return 0;
}
