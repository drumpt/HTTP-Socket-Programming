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
    header_lines = (char *) calloc(header_lines_size + 1, sizeof(char));
    snprintf(
        header_lines,
        header_lines_size,
        "Content-Length: %s\r\n"
        "Connection: close\r\n"
        "\r\n", content_length
    );
    if(content_length) {
        free(content_length);
        content_length = NULL;
    }
    return header_lines;
}

void make_packet(char *packet, const char *status_line, const char *header_lines, const char *body, size_t body_size) {
    size_t status_line_size = strlen(status_line);
    size_t header_lines_size = strlen(header_lines);

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

    printf("Hi~\n");

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

    printf("Hello~\n");

    // 3. Accept client and doing some actions depending on GET or POST method
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_fd;

    int num_bytes;
    char recv_buffer[PACKET_SIZE];
    memset(recv_buffer, 0, PACKET_SIZE);
    FILE *fp;

    while(1) {
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(!fork()) { // child process
            close(sock_fd);

            bool first_packet = true;

            printf("accpeted~\n");

            // fp = fopen("file_4K.bin", "w");
            char *method_outside = calloc(PACKET_SIZE, sizeof(char));
            char *filename_outside = calloc(PACKET_SIZE, sizeof(char));

            while((num_bytes = recv(client_fd, recv_buffer, PACKET_SIZE, 0)) > 0) {
                // char *copied_packet = (char *) calloc(num_bytes, sizeof(char));
                // memcpy(copied_packet, recv_buffer, num_bytes);
                // char *token = strstrtok(copied_packet, "\r\n\r\n");
                // char *header = (char *) calloc(strlen(token) + 5, sizeof(char));
                // memcpy(header, token, strlen(token));
                // memcpy(header + strlen(token), "\r\n\r\n\0", 5);

                // char *copied_header = (char *) calloc(strlen(header), sizeof(char));
                // memcpy(copied_header, header, strlen(header));

                // char *header_token = strstrtok(copied_header, " ");
                // char *method = (char *) calloc(strlen(token), sizeof(char));
                // memcpy(method, header_token, strlen(token));

                // token = strstrtok(NULL, " ");
                // char *filename = (char *) calloc(strlen(header) - 1, sizeof(char));
                // memcpy(filename, token + 1, strlen(token) - 1);

                // // printf("header: %s\nmethod: %s\n", header, method);
                // // printf("filename: %s\n", filename);

                // char *new_copied_header = (char *) calloc(strlen(header), sizeof(char));
                // memcpy(new_copied_header, header, strlen(header));

                // char *new_header_token = strstrtok(new_copied_header, "\r\n");
                // char *body_length = (char *) calloc(strlen(header), sizeof(char));
                // while(new_header_token != NULL) {
                //     // printf("new_header_token: %s\n", new_header_token);
                //     if(strncmp(new_header_token, "Content-Length:", strlen("Content-Length:")) == 0) {
                //         char *body_length_token = strstrtok(new_header_token, ": ");
                //         body_length_token = strstrtok(NULL, ": ");
                //         memcpy(body_length, body_length_token, strlen(body_length_token));
                //         break;
                //     }
                //     new_header_token = strstrtok(NULL, "\r\n");
                // }

                // // printf("body_length: %s\n", body_length);

                // // char *body_length = (char *) calloc()

                // // printf("new_header_token: %s\n", new_header_token);



                // // header = get_header_from_packet(recv_buffer);
                // // method = get_method_from_header(header);
                // // filename = get_filename_from_header(header);

                // // printf("%n%n%n%n%n%n");
                // // printf("header:\n%s\n", header);
                // // // printf("method:\n%s\nmethod_length:\n%d\n", method, strlen(method));
                // // printf("filename:\n%s\n", filename);

                // // printf("%d\n", first_packet);
                // if(first_packet) {
                //     first_packet = false;
                //     memcpy(method_outside, method, strlen(method));
                //     memcpy(filename_outside, filename, strlen(filename));

                //     printf("method_outside: %s\n", method_outside);
                //     printf("method: %s\n", method);

                //     printf("strcmp: %d\n", strcmp(method, "GET"));

                //     if(strcmp(method, "GET") == 0) break;
                //     fp = fopen(filename, "w"); // POST
                //     // printf("%d\n", fp);
                // }

                // // printf("111\n");

                // // printf("1111\n");
                // // // Consider only POST method in this context
                // // char *body_length = get_body_length_from_header(header);
                // // // char *body = (char *) calloc(atoi(body_length) + 1, sizeof(char));

                // // printf("222\n");

                // // memcpy(body, recv_buffer + strlen(header), atoi(body_length));

                // // printf("body:\n%s\n", body);

                // fwrite(recv_buffer + strlen(header), sizeof(char), atoi(body_length), fp);
                // memset(recv_buffer, 0, PACKET_SIZE);
                
                printf("%s\n\n\n\n\n\n\n\n\n\n\n\n", recv_buffer);

                memset(recv_buffer, 0, PACKET_SIZE);
                // free(copied_packet);
                // // free(token);
                // free(header);
                // free(copied_header);
                // // free(header_token);
                // free(method);
                // free(filename);
                // free(new_copied_header);
                // // free(new_header_token);
                // free(body_length);
                // // free(body_length_token);
            }
            printf("receive or post finished1~\n");
            if(strcmp(method_outside, "POST") == 0) fclose(fp);
            printf("receive or post finished2~\n");

            if(strcmp(method_outside, "GET") == 0) { // GET
                fp = fopen(filename_outside, "r");
                if(fp == NULL) { // file doesn't exist
                    char *status_line = make_status_line("404");
                    char *header_lines = make_header_lines(method_outside, 0);
                    char *body = "";
                    char *packet = (char *) calloc(strlen(status_line) + strlen(header_lines) + strlen(body), sizeof(char));
                    make_packet(packet, status_line, header_lines, body, 0);

                    // num_bytes = send(client_fd, packet, PACKET_SIZE, 0);
                    while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                        printf("%s\n", packet);
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                    }

                    // free(status_line);
                    // free(header_lines);
                    // free(body);
                    // free(packet);
                } else { // file exists
                    int MAX_BODY_SIZE = 
                        PACKET_SIZE
                        - 13 // 10 + strlen(code)
                        - 18 + 12 * sizeof(int) + 10 + 14 + strlen("Connection: close\r\n") + 10;
                    char file_buffer[MAX_BODY_SIZE];
                    while((num_bytes = fread(file_buffer, sizeof(char), MAX_BODY_SIZE, fp)) > 0) {
                        char *status_line = make_status_line("200");
                        char *header_lines = make_header_lines(method_outside, num_bytes);
                        char *packet = (char *) calloc(strlen(status_line) + strlen(header_lines) + num_bytes, sizeof(char));
                        make_packet(packet, status_line, header_lines, file_buffer, num_bytes);

                        while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                            printf("%s\n", packet);
                            if(errno == EINTR) continue;
                            else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                        }
                        // free(status_line);
                        // free(header_lines);
                        // free(packet);
                        memset(file_buffer, 0, MAX_BODY_SIZE);
                    }
                }
                fclose(fp);
            } else { // POST or 400 Bad Request
                char *status_line;
                if(strcmp(method_outside, "POST") == 0) {
                    status_line = make_status_line("200");
                } else {
                    status_line = make_status_line("400");
                }
                char *header_lines = make_header_lines(method_outside, 0);
                char *body = "";
                char *packet = (char *) calloc(strlen(status_line) + strlen(header_lines) + strlen(body), sizeof(char));
                make_packet(packet, status_line, header_lines, body, 0);

                printf("status_line : %s\n", status_line);
                printf("header_lines : %s\n", header_lines);
                printf("body : %s\n", body);
                printf("packet : %s\n", packet);

                while((num_bytes = send(client_fd, packet, PACKET_SIZE, 0)) == -1) {
                    if(errno == EINTR) continue;
                    else fprintf(stderr, "Send Error : %s\n", strerror(errno));
                }

                // free(status_line);
                // free(header_lines);
                // free(packet);
            }
            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        else { // parent process
            close(client_fd);
        }
    }
}