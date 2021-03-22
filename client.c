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
#include <stdbool.h>

#define PACKET_SIZE 1024

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

char *get_host_from_url(char *url) { // url : http://ip_address:port_number/test.html
    char **splitted_url, **splitted_host_port;
    int count = split(url, '/', &splitted_url), host_port_count = split(splitted_url[2], ':', &splitted_host_port);
    char *host = splitted_host_port[0]; // server host
    return host;
}

char *get_port_from_url(char *url) {
    char **splitted_url, **splitted_host_port;
    int count = split(url, '/', &splitted_url), host_port_count = split(splitted_url[2], ':', &splitted_host_port);
    char *port = splitted_host_port[1]; // server port
    return port;
}

char *get_filename_from_url(char *url) {
    char **splitted_url;
    int i, count = split(url, '/', &splitted_url);
    char *filename = (char *) calloc(strlen(url) + 1, 0);
    for(int i = 3; i < count; ++i) { // http://host:port/file
        strcat(filename, splitted_url[i]);
        if(i < count - 1) strcat(filename, "/");
    }
    return filename;
}

char *make_request_line(char *method, char *url) {
    char *filename = get_filename_from_url(url);
    int request_line_size = 20 + strlen(filename);
    char *request_line = (char *) calloc(request_line_size + 1, sizeof(char));

    if(strcmp(method, "-G") == 0) { // get
        // int request_line_size = 16 + strlen(filename);
        snprintf(
            request_line,
            request_line_size,
            "GET /%s HTTP/1.0\r\n", filename
        );
    } else { // post
        snprintf(
            request_line,
            request_line_size,
            "POST /%s HTTP/1.0\r\n", filename
        );
    }
    return request_line;
}

char *make_header_lines(char *method, char *url, long long total_body_size) {
    /*
    Implemented : Host, Content-Length, Content-Type
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Connection, and etc.
    */
    int header_lines_size;
    char *header_lines, *content_length = (char *) calloc(12 * sizeof(int) + 10, sizeof(char));
    char *host = get_host_from_url(url);

    if(strcmp(method, "-G") == 0) { // GET
        content_length = "0";
    } else { // POST
        sprintf(content_length, "%lld", total_body_size);
    }
    header_lines_size = 8 + strlen(host) + 18 + strlen(content_length) + strlen("Content-Type: application/octet-stream\r\n") + 10;
    header_lines = (char *) calloc(header_lines_size + 1, sizeof(char));
    snprintf(
        header_lines,
        header_lines_size,
        "Host: %s\r\n"
        "Content-Length: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n", host, content_length
    );
    // if(content_length) free(content_length);
    return header_lines;
}

void make_packet(char *packet, const char *request_line, const char *header_lines, const char *body, const size_t body_size) {
    size_t request_line_size = strlen(request_line);
    size_t header_lines_size = strlen(header_lines);

    if(packet) {
        memcpy(packet, request_line, request_line_size);
        memcpy(packet + request_line_size, header_lines, header_lines_size);
        memcpy(packet + request_line_size + header_lines_size, body, body_size);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3 || (strcmp (argv[1], "-G") != 0 && strcmp (argv[1], "-P") != 0)) {
        printf("Usage:\n        %s -P <URL>        HTTP 1.0 POST from stdin\n"
                "        %s -G <URL>        HTTP 1.0 GET to stdin\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    // 0. Get address information
    struct addrinfo hints, *serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // printf("Hi~\n");

    char *host = get_host_from_url(argv[2]);
    char *port = get_port_from_url(argv[2]);
    if(getaddrinfo(host, port, &hints, &serv_addr) != 0) {
        error("Error : getaddrinfo\n");
    }
    // print_ips(serv_addr);

    // 1. Create socket
    int sock_fd;
    if((sock_fd = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) == -1) {
        error("Error : could not create socket\n");
    }

    // 2. Connect to server
    if(connect(sock_fd, serv_addr->ai_addr, serv_addr->ai_addrlen) == -1) {
        error("Error : connection failed\n");
    }

    // printf("Hello~\n");

    // 3. Read and write packets based on GET or POST method
    int num_bytes, packet_size;
    char recv_buffer[PACKET_SIZE];
    memset(recv_buffer, 0, PACKET_SIZE);

    if(strcmp(argv[1], "-G") == 0) { // GET
        char *request_line = make_request_line(argv[1], argv[2]);
        char *header_lines = make_header_lines(argv[1], argv[2], 0);
        char *body = "";
        int packet_length = strlen(request_line) + strlen(header_lines) + strlen(body);
        char *packet = (char *) calloc(strlen(request_line) + strlen(header_lines) + strlen(body), sizeof(char));
        make_packet(packet, request_line, header_lines, body, 0);

        printf("%s\n", packet);

        int total_bytes = 0, send_bytes = 0;
        while(total_bytes < packet_length) { // send request once
            send_bytes = send(sock_fd, packet + total_bytes, packet_length - total_bytes, 0);
            if(send_bytes < 0) {
                if(errno == EINTR) continue;
                else fprintf(stderr, "Error : %s\n", strerror(errno));
            }
            total_bytes += send_bytes;
        }

        while((num_bytes = send(sock_fd, packet, PACKET_SIZE, 0)) == -1) { // send request once
            if(errno == EINTR) continue;
            else fprintf(stderr, "Send Error : %s\n", strerror(errno));
        }

        // TODO: parse Content-Length and receive until total message length is up to that length
        bool first_packet = true;
        // total_bytes = 0;
        while((num_bytes = recv(sock_fd, recv_buffer, PACKET_SIZE, 0)) > 0) { // receive all packets
            // total_bytes += num_bytes;
            if(first_packet) {
                first_packet = false;

                printf("1~~~");

                // 1. Get header
                char *copied_packet = (char *) calloc(num_bytes, sizeof(char));
                memcpy(copied_packet, recv_buffer, num_bytes);
                char *token = strstrtok(copied_packet, "\r\n\r\n");

                printf("2~~~");

                char *header = (char *) calloc(strlen(token) + 5, sizeof(char));
                memcpy(header, token, strlen(token));
                memcpy(header + strlen(token), "\r\n\r\n\0", 5);

                printf("header: %s\n", header);

                // 2. Get Clontent-Length
                char *copied_header = (char *) calloc(strlen(header), sizeof(char));
                memcpy(copied_header, header, strlen(header));
                char *header_token = strstrtok(copied_packet, "\r\n");
                char *body_length = (char *) calloc(strlen(header), sizeof(char));

                printf("3~~~");

                while(header_token != NULL) {
                    if(strncmp(header_token, "Content-Length:", strlen("Content-Length:")) == 0) {
                        char *body_length_token = strstrtok(header_token, ": ");
                        body_length_token = strstrtok(NULL, ": ");
                        memcpy(body_length, body_length_token, strlen(body_length_token));
                        break;
                    }
                    header_token = strstrtok(NULL, "\r\n");
                }

                printf("body_length: %s\n", body_length);

                fwrite(recv_buffer + strlen(header), sizeof(char), num_bytes - strlen(header), stdout);
            } else {
                fwrite(recv_buffer, sizeof(char), num_bytes, stdout);
            }
            memset(recv_buffer, 0, PACKET_SIZE);
        }
        // if(packet) free(packet);
    } else { // POST
        char *filename = get_filename_from_url(argv[2]);
        int MAX_BODY_SIZE =
            PACKET_SIZE
            - (20 + strlen(filename) + 1)
            - (8 + strlen(host) + 18 + 12 * sizeof(int) + 10 + strlen("Content-Type: application/octet-stream\r\n") + 10);
        char stdin_buffer[MAX_BODY_SIZE], file_buffer[MAX_BODY_SIZE]; // maximum body size
        memset(stdin_buffer, 0, MAX_BODY_SIZE);

        char buffer_file[10];
        memset(buffer_file, 0, sizeof(buffer_file));
        strncpy(buffer_file, "XXXXXXXXXX", 10);
        int fd = mkstemp(buffer_file);
        long long content_length = 0;

        FILE *fp = fdopen(fd, "w");
        while((num_bytes = fread(stdin_buffer, sizeof(char), MAX_BODY_SIZE, stdin)) > 0) {
            content_length += num_bytes;
            fwrite(stdin_buffer, sizeof(char), num_bytes, fp);
            memset(stdin_buffer, 0, MAX_BODY_SIZE);
        }
        fclose(fp);

        FILE *fp_buffer = fopen(buffer_file, "r");
        bool first_packet = true;
        while((num_bytes = fread(file_buffer, sizeof(char), MAX_BODY_SIZE, fp_buffer)) > 0 || content_length == 0) {
            // printf("num_bytes: %d\n", num_bytes);
            int total_bytes = 0, send_bytes = 0;
            if(first_packet) {
                first_packet = false;

                char *request_line = make_request_line(argv[1], argv[2]);
                char *header_lines = make_header_lines(argv[1], argv[2], content_length);
                int packet_length = strlen(request_line) + strlen(header_lines) + num_bytes;
                char *packet = (char *) calloc(packet_length, sizeof(char));
                make_packet(packet, request_line, header_lines, file_buffer, num_bytes);

                while(total_bytes < packet_length) {
                    send_bytes = send(sock_fd, packet + total_bytes, packet_length - total_bytes, 0);
                    if(send_bytes < 0) {
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Error : %s\n", strerror(errno));
                    }
                    total_bytes += send_bytes;
                }
                if(content_length == 0) content_length = -1; // consider empty file (send packet only once)
                // if(packet) free(packet);
            } else {
                while(total_bytes < num_bytes) {
                    send_bytes = send(sock_fd, file_buffer + total_bytes, num_bytes - total_bytes, 0);
                    if(send_bytes < 0) {
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Error : %s\n", strerror(errno));
                    }
                    total_bytes += send_bytes;
                }
            }
            memset(file_buffer, 0, MAX_BODY_SIZE);
        }
        fclose(fp_buffer);
        // TODO(completed): check if this works
        unlink(buffer_file);

        while((num_bytes = recv(sock_fd, recv_buffer, PACKET_SIZE, 0)) > 0) { // receive all packets
            printf("%s\n", recv_buffer);
            memset(recv_buffer, 0, PACKET_SIZE);
        }
        // if(filename) free(filename);
    }
    close(sock_fd);
    exit(EXIT_SUCCESS);
}