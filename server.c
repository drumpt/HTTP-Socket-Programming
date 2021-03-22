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

char *make_header_lines(char *method, long long total_body_size) {
    /*
    Implemented : Content-Length, Connection
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Content-Type, and etc.
    */
    int header_lines_size;
    char *header_lines, *content_length = (char *) calloc(12 * sizeof(int) + 10, sizeof(char));

    if(strcmp(method, "GET") == 0) { // POST
        sprintf(content_length, "%lld", total_body_size);
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

    if(packet) {
        memcpy(packet, status_line, status_line_size);
        memcpy(packet + status_line_size, header_lines, header_lines_size);
        memcpy(packet + status_line_size + header_lines_size, body, body_size);
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

    while(1) {
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(!fork()) { // child process
            close(sock_fd);

            printf("accpeted~\n");

            char *method = calloc(PACKET_SIZE, sizeof(char));
            char *filename = calloc(PACKET_SIZE, sizeof(char));
            char *body_length = calloc(PACKET_SIZE, sizeof(char));
            FILE *fp_post = NULL;

            bool first_packet = true;
            // int total_bytes = 0;
            while((num_bytes = recv(client_fd, recv_buffer, PACKET_SIZE, 0)) > 0) {
                // total_bytes += num_bytes;
                if(first_packet) {
                    first_packet = false;

                    // 1. Get header
                    char *copied_packet = (char *) calloc(num_bytes, sizeof(char));
                    memcpy(copied_packet, recv_buffer, num_bytes);
                    char *token = strstrtok(copied_packet, "\r\n\r\n");
                    char *header = (char *) calloc(strlen(token) + 5, sizeof(char));
                    memcpy(header, token, strlen(token));
                    memcpy(header + strlen(token), "\r\n\r\n\0", 5);

                    // 2. Get method
                    char *copied_header = (char *) calloc(strlen(header), sizeof(char));
                    memcpy(copied_header, header, strlen(header));
                    char *header_token = strstrtok(copied_header, " ");
                    method = (char *) calloc(strlen(token), sizeof(char));
                    memcpy(method, header_token, strlen(token));

                    // 3. Get filename
                    token = strstrtok(NULL, " ");
                    filename = (char *) calloc(strlen(header) - 1, sizeof(char));
                    memcpy(filename, token + 1, strlen(token) - 1);

                    // 4. Get Content-Length
                    char *new_copied_header = (char *) calloc(strlen(header), sizeof(char));
                    memcpy(new_copied_header, header, strlen(header));
                    char *new_header_token = strstrtok(new_copied_header, "\r\n");
                    char *body_length = (char *) calloc(strlen(header), sizeof(char));
                    while(new_header_token != NULL) {
                        // printf("new_header_token: %s\n", new_header_token);
                        if(strncmp(new_header_token, "Content-Length:", strlen("Content-Length:")) == 0) {
                            char *body_length_token = strstrtok(new_header_token, ": ");
                            body_length_token = strstrtok(NULL, ": ");
                            memcpy(body_length, body_length_token, strlen(body_length_token));
                            break;
                        }
                        new_header_token = strstrtok(NULL, "\r\n");
                    }

                    printf("header: %s\nmethod: %s\n", header, method);
                    printf("filename: %s\n", filename);
                    printf("body_length: %s\n", body_length);

                    // free(copied_packet);
                    // free(header);
                    // free(copied_header);
                    // free(new_copied_header);
                    // free(body_length);

                    printf("method: %s\n", method);
                    printf("strcmp: %d\n", strcmp(method, "GET") == 0);

                    if(strcmp(method, "GET") == 0) break;
                    else { // POST
                        fp_post = fopen(filename, "w");
                        fwrite(recv_buffer + strlen(header), sizeof(char), num_bytes - strlen(header), fp_post);
                    }
                    // TODO: free some variables
                } else {
                    fwrite(recv_buffer, sizeof(char), num_bytes, fp_post);
                }
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
            // printf("%d\n", fp_post);
            if(fp_post != NULL) fclose(fp_post);

            printf("receive or post finished1~\n");

            FILE *fp_get = fopen(filename, "r");

            printf("%s\n", method);
            printf("%d\n", strcmp(method, "GET"));


            if(strcmp(method, "GET") == 0 && fp_get != NULL) { // GET : need to send file
                bool first_packet = true;
                int MAX_BODY_SIZE = 
                    PACKET_SIZE
                    - 13 // 10 + strlen(code)
                    - 18 + 12 * sizeof(int) + 10 + 14 + strlen("Connection: close\r\n") + 10;
                char file_buffer[MAX_BODY_SIZE];
                memset(file_buffer, 0, MAX_BODY_SIZE);

                // get length of file
                long long content_length = 0;
                long long pos = ftello(fp_get);
                printf("pos: %lld\n", pos);
                fseeko(fp_get, 0, SEEK_END);
                content_length = ftello(fp_get);
                fseeko(fp_get, pos, SEEK_SET);
                printf("content_length: %lld\n", content_length);

                while((num_bytes = fread(file_buffer, sizeof(char), MAX_BODY_SIZE, fp_get)) > 0 || content_length == 0) {
                    int total_bytes = 0, send_bytes = 0;
                    if(first_packet) {
                        first_packet = false;

                        char *status_line = make_status_line("200");
                        char *header_lines = make_header_lines(method, content_length);
                        int packet_length = strlen(status_line) + strlen(header_lines) + num_bytes;
                        char *packet = (char *) calloc(packet_length, sizeof(char));
                        make_packet(packet, status_line, header_lines, file_buffer, num_bytes);

                        while(total_bytes < packet_length) {
                            send_bytes = send(client_fd, packet + total_bytes, packet_length - total_bytes, 0);
                            if(send_bytes < 0) {
                                if(errno == EINTR) continue;
                                else fprintf(stderr, "Error : %s\n", strerror(errno));
                            }
                            total_bytes += send_bytes;
                        }
                        if(content_length == 0) content_length = -1; // consider empty file (send packet only once)
                    }
                    else {
                        while(total_bytes < num_bytes) {
                            send_bytes = send(client_fd, file_buffer + total_bytes, num_bytes - total_bytes, 0);
                            if(send_bytes < 0) {
                                if(errno == EINTR) continue;
                                else fprintf(stderr, "Error : %s\n", strerror(errno));
                            }
                            total_bytes += send_bytes;
                        }
                    }
                    memset(file_buffer, 0, MAX_BODY_SIZE);                    
                }
            } else { // POST(OK) or 404 Not Found or 400 Bad Request
                char *status_line;
                if(strcmp(method, "POST") == 0) status_line = make_status_line("200");
                else if(fp_get == NULL) status_line = make_status_line("404");
                else status_line = make_status_line("400");
                char *header_lines = make_header_lines(method, 0);
                char *body = "";
                int packet_length = strlen(status_line) + strlen(header_lines) + strlen(body);
                char *packet = (char *) calloc(packet_length, sizeof(char));
                make_packet(packet, status_line, header_lines, body, 0);

                int total_bytes = 0, send_bytes = 0;
                while(total_bytes < packet_length) {
                    send_bytes = send(client_fd, packet + total_bytes, packet_length - total_bytes, 0);
                    if(send_bytes < 0) {
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Error : %s\n", strerror(errno));
                    }
                    total_bytes += send_bytes;
                }
            }
            close(client_fd);
            exit(EXIT_SUCCESS);
        } else { // parent process
            close(client_fd);
        }
    }
}