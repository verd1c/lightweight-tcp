#include "microtcp.h"
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>

int main(){

    microtcp_sock_t s = microtcp_socket(AF_INET, SOCK_DGRAM, 0);

    printf("Socket created at: %d\n", s.sd);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8080);
    sin.sin_addr.s_addr = INADDR_ANY;
    //sin.sin_addr.s_addr = htonl(INADDR_ANY);


    microtcp_bind(&s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));

    char in[256] = "saccascascasvasgbsabsdfbsdbdbds";
    int rec;
    socklen_t addrln = sizeof(sin);
    sendto(
        s.sd,
        (const void*)in,
        256,
        0,
        (struct sockaddr *)&sin,
        addrln
    );

    // int v;
    // int mysock = socket(AF_INET, SOCK_STREAM, 0);
    // struct sockaddr_in myaddr;
    // memset(&myaddr, 0, sizeof(struct sockaddr_in));
    // myaddr.sin_family = AF_INET;
    // myaddr.sin_port = htons(25565);
    // //sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    // myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // bind(mysock, (struct sockaddr *)&myaddr, sizeof(struct sockaddr_in));
    // listen(mysock, v);

    while(1){

    }
    

    return 0;
}