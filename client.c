#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main (int argc, char *argv[]) {
    if (argc < 3 || (strcmp (argv[1], "-G") != 0 && strcmp (argv[1], "-P") != 0)) {
        printf("Usage:\n        %s -P <URL>        HTTP 1.0 POST from stdin\n"
                "        %s -G <URL>        HTTP 1.0 GET to stdin\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}