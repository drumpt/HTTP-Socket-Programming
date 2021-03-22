#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <netdb.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/stat.h>

#define PACKET_SIZE 1024
#define BACKLOG 30

void error(const char *msg);
int split(const char *txt, char delim, char ***tokens);
char *strstrtok(char *src, char *sep);
void print_ips(struct addrinfo *lst);
void make_packet(char *packet, const char *status_line, const char *header_lines, const char *body, size_t body_size);

#endif