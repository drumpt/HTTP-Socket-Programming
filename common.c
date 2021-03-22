#include "common.h"

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

void make_packet(char *packet, const char *status_line, const char *header_lines, const char *body, size_t body_size) {
    size_t status_line_size = strlen(status_line);
    size_t header_lines_size = strlen(header_lines);

    if(packet) {
        memcpy(packet, status_line, status_line_size);
        memcpy(packet + status_line_size, header_lines, header_lines_size);
        memcpy(packet + status_line_size + header_lines_size, body, body_size);
    }
}