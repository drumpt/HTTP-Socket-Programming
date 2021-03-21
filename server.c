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
    char *copied_packet = calloc(strlen(packet) + 1, sizeof(char));
    if(copied_packet) {
        memcpy(copied_packet, packet, strlen(packet));

        char *token = strstrtok(copied_packet, "\r\n\r\n");
        char *header = calloc(strlen(copied_packet), sizeof(char));
        memcpy(header, token, strlen(token));
        memcpy(header + strlen(token), "\r\n\r\n", 4);

        free(copied_packet);
        return header;
    }
}

char *get_method_from_header(char *header) {
    char *copied_header = calloc(strlen(header) + 1, sizeof(char));
    if(copied_header) {
        memcpy(copied_header, header, strlen(header));

        char *token = strstrtok(copied_header, " ");
        char *method = calloc(strlen(copied_header) + 1, sizeof(char));
        memcpy(method, token, strlen(token));

        free(copied_header);
        return method;
    }
}

char *get_filename_from_header(char *header) {
    char *copied_header = calloc(strlen(header) + 1, sizeof(char));
    if(copied_header) {
        memcpy(copied_header, header, strlen(header));

        char *token = strstrtok(copied_header, " ");
        char *filename = calloc(strlen(copied_header) + 1, sizeof(char));
        token = strstrtok(NULL, " ");
        memcpy(filename, token + 1, strlen(token) - 1);

        free(copied_header);
        return filename;
    }
}

char *get_body_length_from_header(char *header) {
    char *copied_header = calloc(strlen(header) + 1, sizeof(char));
    if(copied_header) {
        memcpy(copied_header, header, strlen(header));

        char *token = strstrtok(copied_header, "\r\n");
        char *body_length = calloc(strlen(copied_header), sizeof(char));
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

char *make_status_line(char *code) {
    int status_line_size = 27;
    char *status_line = calloc(status_line_size + 1, sizeof(char));

    char *message;
    if(strcmp(code, "200") == 0) { // OK
        message = "OK";
    } else if(strcmp(code, "404") == 0) { // Not Found
        message = "Not Found";
    } else { // Bad Request
        message = "Bad Request";
    }

    snprintf(
        status_line,
        status_line_size,
        "HTTP/1.0 %s %s\r\n", code, message
    );
    return status_line;
}

char *make_header_lines(char *method, int buffer_size) {
    /*
    Implemented : Content-Length, Connection
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Content-Type, and etc.
    */
    int header_lines_size;
    char *header_lines, *content_length = (char *) calloc(12 * sizeof(int) + 10, sizeof(char));

    if(strcmp(method, "GET") == 0) { // POST
        sprintf(content_length, "%d", buffer_size);
    } else { // GET
        content_length = "0";
    }
    header_lines_size = 18 + strlen(content_length) + 14 + strlen("Connection: close\r\n") + 10;
    header_lines = calloc(header_lines_size + 1, sizeof(char));
    snprintf(
        header_lines,
        header_lines_size,
        "Content-Length: %s\r\n"
        "Connection: close\r\n"
        "\r\n", content_length
    );
    free(content_length);
    return header_lines;
}

void make_packet(char *packet, const char *status_line, const char *header_lines, const char *body) {
    size_t status_line_size = strlen(status_line);
    size_t header_lines_size = strlen(header_lines);
    size_t body_size = strlen(body);

    printf("status_line_size: %d\nheader_lines_size: %d\nbody_size: %d\n", status_line_size, header_lines_size, body_size);

    if(packet) {
        memcpy(packet, status_line, status_line_size);
        memcpy(packet + status_line_size, header_lines, header_lines_size);
        memcpy(packet + status_line_size + header_lines_size, body, body_size);
        printf("packet_inner_function:\n%s\n", packet);
    }
    printf("packet_inner_length:%d\n", strlen(packet));
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
    if(bind(sock_fd, serv_addr->ai_addr, serv_addr->ai_addrlen) == -1) {
        error("Error : could not bind");
    }

    if(listen(sock_fd, BACKLOG) == -1) {
        error("Error : could not listen");
    }

    // 3. Accept client and doing some actions depending on GET or POST method
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_fd;

    int num_bytes;
    char recv_buffer[PACKET_SIZE];
    bool first_packet = true;

    while(1) {
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(!fork()) { // child process
            close(sock_fd);

            char *header, *method, *filename;
            char *body_length, *body;
            FILE *fp;

            while((num_bytes = recv(client_fd, recv_buffer, PACKET_SIZE, 0)) > 0) {
                if(first_packet) {
                    first_packet = false;

                    header = get_header_from_packet(recv_buffer);
                    method = get_method_from_header(header);
                    filename = get_filename_from_header(header);

                    if(strcmp(method, "GET") == 0) break;
                    fp = fopen(filename, "w");
                }

                // Consider only POST method in this context
                char *body_length = get_body_length_from_header(header);
                char *body = calloc(atoi(body_length) + 1, sizeof(char));
                memcpy(body, recv_buffer + strlen(header), atoi(body_length));
                fputs(body, fp); // TODO: sizeof(buffer) or strlen(buffer)?

                free(body);
                memset(recv_buffer, 0, PACKET_SIZE);
            }

            if(strcmp(method, "GET") == 0) { // GET
                fp = fopen(filename, "r");
                if(fp == NULL) { // file doesn't exist
                    char *status_line = make_status_line("404");
                    char *header_lines = make_header_lines(method, 0);
                    char *body = "";
                    char *packet = calloc(strlen(status_line) + strlen(header_lines) + strlen(body) + 1, sizeof(char));
                    make_packet(packet, status_line, header_lines, body);

                    while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                    }

                    free(status_line);
                    free(header_lines);
                    free(packet);
                } else { // file exists
                    int MAX_BODY_SIZE = 
                        PACKET_SIZE
                        - 13 // 10 + strlen(code)
                        - 18 + 12 * sizeof(int) + 10 + 14 + strlen("Connection: close\r\n") + 10;
                    char file_buffer[MAX_BODY_SIZE];
                    while(fgets(file_buffer, MAX_BODY_SIZE, fp) != NULL) {
                        char *status_line = make_status_line("200");
                        char *header_lines = make_header_lines(method, strlen(file_buffer));
                        char *packet = calloc(strlen(status_line) + strlen(header_lines) + strlen(file_buffer) + 1, sizeof(char));
                        make_packet(packet, status_line, header_lines, file_buffer);

                        while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                            if(errno == EINTR) continue;
                            else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                        }
                        free(status_line);
                        free(header_lines);
                        free(packet);
                        memset(file_buffer, 0, MAX_BODY_SIZE);
                    }
                }
                fclose(fp);
            } else { // POST or 400 Bad Request
                char *status_line;
                if(strcmp(method, "POST") == 0) status_line = make_status_line("200");
                else status_line = make_status_line("400");
                char *header_lines = make_header_lines(method, 0);
                char *body = "";
                char *packet = calloc(strlen(status_line) + strlen(header_lines) + strlen(body) + 1, sizeof(char));
                make_packet(packet, status_line, header_lines, body);

                while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                    if(errno == EINTR) continue;
                    else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                }

                free(status_line);
                free(header_lines);
                free(packet);
            }
            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        else { // parent process
            close(client_fd);
        }
    }
}