#define PACKET_SIZE 1024

typedef struct Packet {
    char* request_line;
    char* header;
    char* body;
} Packet;