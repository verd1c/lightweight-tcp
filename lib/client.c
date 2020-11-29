#include "microtcp.h"
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>

int main(){

    microtcp_sock_t s = microtcp_socket(AF_INET, SOCK_DGRAM, 0);

    //printf("Socket created at: %d\n", s.sd);
    char *hello = "Hello from client"; 
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8080);
    sin.sin_addr.s_addr = INADDR_ANY;
    //sin.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("%d, %d\n", sizeof(struct sockaddr), sizeof(socklen_t));
    //microtcp_bind(&s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    s.address = sin;
    s.address_len = sizeof(struct sockaddr_in);

    microtcp_connect(&s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));

    if(DEBUG) sleep(5);
 
    // Shutdown
    microtcp_shutdown(&s, SHUT_RDWR);

    return 0;
}