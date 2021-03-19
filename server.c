#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main (int argc, char *argv[]) {
    if(argc < 3 || strcmp (argv[1], "-p") != 0) {
        printf ("Usage: %s -p <port>\nServes and creates files under the current directory, no URL encoding supported\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}