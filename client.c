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
    int count = split(url, '/', &splitted_url);
    char *filename = splitted_url[count - 1];
    return filename;
}

char *make_request_line(char *method, char *url) {
    char *filename = get_filename_from_url(url);
    int request_line_size = 20 + strlen(filename);
    char *request_line = malloc(request_line_size + 1);

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

char *make_header_lines(char *method, char *url, int buffer_size) {
    /*
    Implemented : Host, Content-Length, Content-Type
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Connection, and etc.
    */
    int header_lines_size;
    char *header_lines, *content_length = (char *) malloc(12 * sizeof(int) + 10);
    char *host = get_host_from_url(url);

    if(strcmp(method, "-G") == 0) { // GET
        content_length = "0";
    } else { // POST
        sprintf(content_length, "%d", buffer_size);
    }
    header_lines_size = 8 + strlen(host) + 18 + strlen(content_length) + strlen("Content-Type: application/octet-stream\r\n") + 10;
    header_lines = malloc(header_lines_size + 1);
    snprintf(
        header_lines,
        header_lines_size,
        "Host: %s\r\n"
        "Content-Length: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n", host, content_length
    );
    return header_lines;
}

void make_packet(char *packet, const char *request_line, const char *header_lines, const char *body) {
    size_t request_line_size = strlen(request_line);
    size_t header_lines_size = strlen(header_lines);
    size_t body_size = strlen(body);

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

    // 3. Read and write packets based on GET or POST method
    int n;
    char recv_buffer[PACKET_SIZE];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    if(strcmp(argv[1], "-G") == 0) { // GET
        char *request_line = make_request_line(argv[1], argv[2]);
        char *header_lines = make_header_lines(argv[1], argv[2], 0);
        char *body = "";
        char *packet = malloc(strlen(request_line) + strlen(header_lines) + strlen(body) + 1);
        make_packet(packet, request_line, header_lines, body);

        while((n = send(sock_fd, packet, strlen(packet), 0)) == -1) {
            if(errno == EINTR) continue;
            else fprintf(stderr, "Send Error : %s\n", strerror(errno));
        }

        while((n = recv(sock_fd, recv_buffer, sizeof(recv_buffer), 0)) > 0) {
            printf(recv_buffer);
        }
    } else { // POST
        char stdin_buffer[PACKET_SIZE];
        memset(stdin_buffer, 0, sizeof(stdin_buffer));

        while(fgets(stdin_buffer, PACKET_SIZE, stdin) != NULL) {
            char *request_line = make_request_line(argv[1], argv[2]);
            char *header_lines = make_header_lines(argv[1], argv[2], strlen(stdin_buffer));
            char *packet = malloc(strlen(request_line) + strlen(header_lines) + strlen(stdin_buffer) + 1);
            make_packet(packet, request_line, header_lines, stdin_buffer);

            while((n = send(sock_fd, packet, strlen(packet), 0)) == -1) {
                if(errno == EINTR) continue;
                else fprintf(stderr, "Send Error : %s\n", strerror(errno));
            }

            while((n = recv(sock_fd, recv_buffer, sizeof(recv_buffer), 0)) > 0) {
                printf(recv_buffer);
            }
        }
    }
    exit(EXIT_SUCCESS);
}