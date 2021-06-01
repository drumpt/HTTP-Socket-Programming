#include "common.h"

bool is_valid_request(char *request, size_t request_size) {
    // 1. Check if request consists of header and body
    char *copied_packet = (char *) calloc(request_size, sizeof(char));
    memcpy(copied_packet, request, request_size);
    char *token = strstrtok(copied_packet, "\r\n\r\n");

    if(token == NULL) return false; // not consists of header and body

    char *header = (char *) calloc(strlen(token) + 5, sizeof(char));
    memcpy(header, token, strlen(token));
    memcpy(header + strlen(token), "\r\n\r\n\0", 5);

    // 2. Check if method is valid
    char *copied_header = (char *) calloc(strlen(header), sizeof(char));
    memcpy(copied_header, header, strlen(header));
    char *header_token = strstrtok(copied_header, " ");
    char *method = (char *) calloc(strlen(token), sizeof(char));
    memcpy(method, header_token, strlen(token));

    if(strncmp(method, "POST", 4) != 0 && strncmp(method, "GET", 3) != 0) return false;

    // 3. Check if filename startswith  "/"
    token = strstrtok(NULL, " ");
    if(strncmp(token, "/", 1) != 0) return false;

    // 4. Check if request line endswith "HTTP/1.0\r\n"
    token = strstrtok(NULL, " ");
    if(strncmp(token, "HTTP/1.0\r\n", 10) != 0) return false;

    // 5. Check Content-Length
    char *new_copied_header = (char *) calloc(strlen(header), sizeof(char));
    memcpy(new_copied_header, header, strlen(header));
    char *new_header_token = strstrtok(new_copied_header, "\r\n");
    char *body_length = (char *) calloc(strlen(header), sizeof(char));
    while(new_header_token != NULL) {
        if(strncmp(new_header_token, "Content-Length:", strlen("Content-Length:")) == 0) {
            char *body_length_token = strstrtok(new_header_token, ": ");
            body_length_token = strstrtok(NULL, ": ");
            memcpy(body_length, body_length_token, strlen(body_length_token));
            break;
        }
        new_header_token = strstrtok(NULL, "\r\n");
    }
    if(strncmp(method, "POST", 4) == 0 && strcmp(body_length, "") == 0) return false;
    return true;
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

char *make_header_lines(char *method, long long body_size) {
    /*
    Implemented : Content-Length, Connection
    Not implemented : User-Agent, Accept, Accept-Language, Accept-Encoding, Accept-Charset, Keep-Alive, Content-Type, and etc.
    */
    int header_lines_size;
    char *body_size_str = (char *) calloc(12 * sizeof(int) + 10, sizeof(char));

    if(strcmp(method, "GET") == 0) { // POST
        sprintf(body_size_str, "%lld", body_size);
    } else { // GET
        body_size_str = "0";
    }
    header_lines_size = 18 + strlen(body_size_str) + 14 + strlen("Connection: close\r\n") + 10;
    char *header_lines = (char *) calloc(header_lines_size + 1, sizeof(char));
    snprintf(
        header_lines,
        header_lines_size,
        "Content-Length: %s\r\n"
        "Connection: close\r\n"
        "\r\n", body_size_str
    );
    return header_lines;
}

void make_directory(char *filename) {
    char *dir_to_make = calloc(strlen(filename), sizeof(char));
    char **splitted_filename;
    int count = split(filename, '/', &splitted_filename);
    int i;
    for(i = 0; i < count - 1; ++i) { // inclues only directories
        strcat(dir_to_make, splitted_filename[i]);
        if(i != count - 2) strcat(dir_to_make, "/");
        struct stat st = {0};
        if(stat(dir_to_make, &st) == -1) {
            mkdir(dir_to_make, 0777);
        }
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

    char *host = "0.0.0.0";
    char *port = argv[2];

    if(getaddrinfo(host, port, &hints, &serv_addr) != 0) {
        error("Error : getaddrinfo\n");
    }
    // print_ips(serv_addr);

    // 1. Create socket and apply SO_REUSEADDR
    int sock_fd;
    if((sock_fd = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) == -1) {
        error("Error : could not create socket");
    }

    int option = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

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
    memset(recv_buffer, 0, PACKET_SIZE);

    while(1) {
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(!fork()) { // child process
            close(sock_fd);

            char *method = calloc(PACKET_SIZE, sizeof(char));
            char *filename = calloc(PACKET_SIZE, sizeof(char));
            char *body_length = calloc(PACKET_SIZE, sizeof(char));
            FILE *fp_post = NULL;

            bool first_packet = true;
            bool valid = true;
            long long total_receive_body_length = 0;
            while((num_bytes = recv(client_fd, recv_buffer, PACKET_SIZE, 0)) > 0) {
                if(first_packet) {
                    first_packet = false;

                    // 0. Check whether request message is valid or not using regular expression
                    if(!is_valid_request(recv_buffer, num_bytes)) {
                        valid = false;
                        break;
                    }

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
                    body_length = (char *) calloc(strlen(header), sizeof(char));
                    while(new_header_token != NULL) {
                        if(strncmp(new_header_token, "Content-Length:", strlen("Content-Length:")) == 0) {
                            char *body_length_token = strstrtok(new_header_token, ": ");
                            body_length_token = strstrtok(NULL, ": ");
                            memcpy(body_length, body_length_token, strlen(body_length_token));
                            break;
                        }
                        new_header_token = strstrtok(NULL, "\r\n");
                    }
                    total_receive_body_length += (num_bytes - strlen(header));

                    free(copied_packet);
                    free(copied_header);
                    free(new_copied_header);

                    if(strcmp(method, "GET") == 0) break;
                    else { // POST
                        make_directory(filename);
                        fp_post = fopen(filename, "w");
                        fwrite(recv_buffer + strlen(header), sizeof(char), num_bytes - strlen(header), fp_post);
                    }
                } else {
                    fwrite(recv_buffer, sizeof(char), num_bytes, fp_post);
                    total_receive_body_length += num_bytes;
                }
                memset(recv_buffer, 0, PACKET_SIZE);
                if(total_receive_body_length >= atoll(body_length)) break;
            }
            if(fp_post != NULL) fclose(fp_post);
            
            FILE *fp_get = fopen(filename, "r");
            first_packet = true;
            if(strcmp(method, "GET") == 0 && fp_get != NULL) { // GET : need to send file
                int MAX_BODY_SIZE = 
                    PACKET_SIZE
                    - 13 // 10 + strlen(code)
                    - 18 + 12 * sizeof(int) + 10 + 14 + strlen("Connection: close\r\n") + 10;
                char file_buffer[MAX_BODY_SIZE];
                memset(file_buffer, 0, MAX_BODY_SIZE);

                // get length of file in advance
                long long content_length = 0;
                long long pos = ftello(fp_get);
                fseeko(fp_get, 0, SEEK_END);
                content_length = ftello(fp_get);
                fseeko(fp_get, pos, SEEK_SET);

                while((num_bytes = fread(file_buffer, sizeof(char), MAX_BODY_SIZE, fp_get)) > 0 || content_length == 0) {
                    int total_send_bytes = 0, send_bytes = 0;
                    if(first_packet) {
                        first_packet = false;

                        char *status_line = make_status_line("200");
                        char *header_lines = make_header_lines(method, content_length);
                        int packet_length = strlen(status_line) + strlen(header_lines) + num_bytes;
                        char *packet = (char *) calloc(packet_length, sizeof(char));
                        make_packet(packet, status_line, header_lines, file_buffer, num_bytes);

                        while(total_send_bytes < packet_length) {
                            send_bytes = send(client_fd, packet + total_send_bytes, packet_length - total_send_bytes, 0);
                            if(send_bytes < 0) {
                                if(errno == EINTR) continue;
                                else fprintf(stderr, "Error : %s\n", strerror(errno));
                            }
                            total_send_bytes += send_bytes;
                        }
                        if(content_length == 0) content_length = -1; // consider empty file (send packet only once)
                        free(packet);
                    }
                    else {
                        while(total_send_bytes < num_bytes) {
                            send_bytes = send(client_fd, file_buffer + total_send_bytes, num_bytes - total_send_bytes, 0);
                            if(send_bytes < 0) {
                                if(errno == EINTR) continue;
                                else fprintf(stderr, "Error : %s\n", strerror(errno));
                            }
                            total_send_bytes += send_bytes;
                        }
                    }
                    memset(file_buffer, 0, MAX_BODY_SIZE);                    
                }
            } else { // POST(OK) or 404 Not Found or 400 Bad Request
                char *status_line;
                if(!valid) make_status_line("400");
                else if(strcmp(method, "POST") == 0) status_line = make_status_line("200");
                else if(fp_get == NULL) status_line = make_status_line("404");
                else status_line = make_status_line("400");
                char *header_lines = make_header_lines(method, 0);
                char *body = "";
                int packet_length = strlen(status_line) + strlen(header_lines) + strlen(body);
                char *packet = (char *) calloc(packet_length, sizeof(char));
                make_packet(packet, status_line, header_lines, body, 0);

                int total_send_bytes = 0, send_bytes = 0;
                while(total_send_bytes < packet_length) {
                    send_bytes = send(client_fd, packet + total_send_bytes, packet_length - total_send_bytes, 0);
                    if(send_bytes < 0) {
                        if(errno == EINTR) continue;
                        else fprintf(stderr, "Error : %s\n", strerror(errno));
                    }
                    total_send_bytes += send_bytes;
                }
                free(packet);
            }
            free(method);
            free(filename);
            free(body_length);

            if(fp_get != NULL) fclose(fp_get);
            close(client_fd);
            exit(EXIT_SUCCESS);
        } else { // parent process
            close(client_fd);
        }
    }
}