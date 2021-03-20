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

#include <netdb.h>
#include <assert.h>
#include <stdbool.h>

#define PACKET_SIZE 1024
#define BACKLOG 30

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int split(const char *txt, char delim, char ***tokens) {
    int *tklen, *t, count = 1;
    char **arr, *p = (char *) txt;

    while (*p != '\0') if (*p++ == delim) count += 1;
    t = tklen = calloc (count, sizeof (int));
    for (p = (char *) txt; *p != '\0'; p++) *p == delim ? *t++ : (*t)++;
    *tokens = arr = malloc (count * sizeof (char *));
    t = tklen;
    p = *arr++ = calloc (*(t++) + 1, sizeof (char *));
    while (*txt != '\0')
    {
        if (*txt == delim)
        {
            p = *arr++ = calloc (*(t++) + 1, sizeof (char *));
            txt++;
        }
        else *p++ = *txt++;
    }
    free (tklen);
    return count;
}

void print_ips(struct addrinfo *lst) {
    /* IPv4 */
    char ipv4[INET_ADDRSTRLEN];
    struct sockaddr_in *addr4;
    
    /* IPv6 */
    char ipv6[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *addr6;
    
    for (; lst != NULL; lst = lst->ai_next) {
        if (lst->ai_addr->sa_family == AF_INET) {
            addr4 = (struct sockaddr_in *) lst->ai_addr;
            inet_ntop(AF_INET, &addr4->sin_addr, ipv4, INET_ADDRSTRLEN);
            printf("IP: %s\n", ipv4);
        }
        else if (lst->ai_addr->sa_family == AF_INET6) {
            addr6 = (struct sockaddr_in6 *) lst->ai_addr;
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipv6, INET6_ADDRSTRLEN);
            printf("IP: %s\n", ipv6);
        }
    }
}

char *strstrtok(char *src, char *sep) {
    static char *str = NULL;
    if (src) str = src;
    else src = str;

    if (str == NULL) return NULL;
    char *next = strstr(str, sep);
    if (next) {
        *next = '\0';
        str = next + strlen(sep);
    }
    else str = NULL;
    return src;
}

char *get_header_from_packet(char *packet) {
    char *copied_packet = malloc(strlen(packet) + 1);
    if(copied_packet) {
        memcpy(copied_packet, packet, strlen(packet));

        char *token = strstrtok(copied_packet, "\r\n\r\n");
        char *header = malloc(strlen(copied_packet));
        memcpy(header, token, strlen(token));
        memcpy(header + strlen(token), "\r\n\r\n", 4);

        free(copied_packet);
        return header;
    }
}

char **get_method_from_header(char *header) {
    char *copied_header = malloc(strlen(header) + 1);
    if(copied_header) {
        memcpy(copied_header, header, strlen(header));
    }
}

char *get_filename_from_header(char *header) {

}

char *get_body_length_from_header(char *header) {
    char *copied_header = malloc(strlen(header) + 1);
    if(copied_header) {
        memcpy(copied_header, header, strlen(header));

        char *token = strstrtok(copied_header, "\r\n");
        char *body_length = malloc(strlen(copied_header));
        while(token != NULL) {
            if(strncmp(token, "Content-Length:", strlen("Content-Length:")) == 0) {
                char *body_length_token = strstrtok(token, ": ");
                body_length_token = strstrtok(NULL, ": ");
                memcpy(body_length, body_length_token, strlen(body_length_token));
            }
            token = strstrtok(NULL, "\r\n");
        }

        free(copied_header);
        return body_length;
    }
}

char *get_body_from_packet(char *packet, size_t body_length) {
    char *copied_packet = malloc(strlen(packet) + 1);
    if(copied_packet) {
        memcpy(copied_packet, packet, strlen(packet));

        char *token = strstrtok(copied_packet, "\r\n\r\n");
        char *body = malloc(strlen(copied_packet));

        token = strstrtok(NULL, "\r\n\r\n"); // ignore header
        memcpy(body, token, body_length);

        free(copied_packet);
        return body;
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3 || strcmp (argv[1], "-p") != 0) {
        printf ("Usage: %s -p <port>\nServes and creates files under the current directory, no URL encoding supported\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 0. Get address information
    struct addrinfo hints, *serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char *host = "localhost";
    char *port = argv[2];

    if(getaddrinfo(host, port, &hints, &serv_addr) != 0) {
        error("Error : getaddrinfo\n");
    }

    // 1. Create socket
    int sock_fd;
    if((sock_fd = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) == -1) {
        error("Error : could not create socket");
    }
    // print_ips(serv_addr);

    // 2. Bind and listen
    bind(sock_fd, serv_addr->ai_addr, serv_addr->ai_addrlen);
    listen(sock_fd, BACKLOG);

    // 3. Accept client and doing some actions depending on GET or POST method
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_fd;

    int num_bytes;
    char recv_buffer[PACKET_SIZE];

    /*
    GET /test.html HTTP/1.0
    Host: localhost
    Content-Length: 0
    Content-Type: application/octet-stream

    POST /test.html HTTP/1.0
    Host: localhost
    Content-Length: 29
    Content-Type: application/octet-stream

    ����zm���3F�d��N�հQx�-
    */

    while(1) {
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(!fork()) { // child process
            close(sock_fd);

            while((num_bytes = recv(client_fd, recv_buffer, PACKET_SIZE, 0)) > 0) {
                char *header = get_header_from_packet(recv_buffer);
                // char *method = get_method_from_header(header);
                // char *filename = get_filename_from_header(filename);

                // printf("\n\n\n\n\n\n%s\n", header);

                char *body_length = get_body_length_from_header(header);

                // printf("%s\n", body_length);

                char *body = get_body_from_packet(recv_buffer, (size_t) atoi(body_length));

                // printf("%s\n\n\n\n\n\n\n\n", body);

                printf("%s\n", recv_buffer);

                memset(header, 0, strlen(header));
                memset(body_length, 0, strlen(header));
                memset(recv_buffer, 0, PACKET_SIZE);
            }
            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        close(client_fd); // parent process
    }
}