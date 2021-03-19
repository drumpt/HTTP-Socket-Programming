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

#include "common.h"

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

char *concat(const char *s1, const char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

char *make_request_line(char *method, char *url) { // url : http://ip_address:port_number/test.html
    if(strcmp(method, "-G") != 0 && strcmp(method, "-P") != 0) return ""; // wrong method

    char *request_line;
    char **splitted_url;
    int i, request_line_size, count = split(url, '/', &splitted_url);
    if(count < 4) return ""; // invliad url (no file)
    char *filename = splitted_url[count - 1];

    if(strcmp(method, "-G") == 0) { // get
        request_line_size = 16 + strlen(filename);
        request_line = malloc(request_line_size + 1);
        snprintf(
            request_line,
            request_line_size,
            "GET /%s HTTP/1.0\r\n", filename
        );
    } else { // post
        request_line_size = 17 + strlen(filename);
        request_line = malloc(request_line_size + 1);
        snprintf(
            request_line,
            request_line_size,
            "POST /%s HTTP/1.0\r\n", filename
        );
    }
    return request_line;
}

char *make_header_lines(char *method, char *url) {
    /*
    Implemented : Host, Content-Length
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Connection
    */
    char *header_lines, *content_length;
    char **splitted_url, **splitted_host_port;
    int i, header_lines_size, count = split(url, '/', &splitted_url), host_port_count = split(splitted_url[2], ':', &splitted_host_port);
    if(host_port_count != 2) return "";
    char *host = splitted_host_port[0]; // server host
    
    if(strcmp(method, "-G") == 0) { // GET
        content_length = "0";
    } else { // POST
        // struct stat st;
        // stat(filename, &st);
        // char *content_length = st.st_size;
        content_length = "0";
    }
    header_lines_size = 8 + strlen(host) + 18 + strlen(content_length) + 2;
    header_lines = malloc(header_lines_size+ 1);
    snprintf(
        header_lines,
        header_lines_size,
        "Host: %s\r\n"
        "Content-Length: %s\r\n"
        "\r\n", host, content_length
    );
    return header_lines;
}

Packet make_packet(char *request_line, char *url) {

}

int main(int argc, char *argv[]) {
    printf("%d\n", argc);
    if(argc < 3 || (strcmp (argv[1], "-G") != 0 && strcmp (argv[1], "-P") != 0)) {
        printf("Usage:\n        %s -P <URL>        HTTP 1.0 POST from stdin\n"
                "        %s -G <URL>        HTTP 1.0 GET to stdin\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    char *request_line = make_request_line(argv[1], argv[2]);
    if(strcmp(request_line, "") == 0) {
        error("Incorrect method or URL");
        exit(EXIT_FAILURE);
    }
    char *header_lines = make_header_lines(argv[1], argv[2]);
    if(strcmp(header_lines, "") == 0) {
        error("Incorrect <host>:<port>");
        exit(EXIT_FAILURE);
    }

    FILE *std_fd = stdin;
    FILE *socket_fd = fdopen(dup(fileno(std_fd)), "r");

    // unsigned char *readStdin = (unsigned char *) calloc(PACKET_SIZE * sizeof(unsigned char), 1);
    char buffer[PACKET_SIZE];

    // unsigned char *buffer = (unsigned char *) calloc(PACKET_SIZE * sizeof(unsigned char), 1);

    uint64_t content_length = 0;
    while(fgets(buffer, PACKET_SIZE, std_fd) != NULL) {
    // while(fread(buffer, 1, PACKET_SIZE, std_fd)) {
        if(buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';
        printf("%d %d\n", strlen(buffer), PACKET_SIZE);
        printf("%d\n", strlen(buffer));
        content_length += (uint64_t) strlen(buffer);
    }
    fclose(std_fd);
    // printf("%lld\n", content_length);

    exit(EXIT_SUCCESS);
}