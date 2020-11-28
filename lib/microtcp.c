/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "microtcp.h"
#include "../utils/crc32.h"

microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t sock;
  int sock_desc;
  
  // Create socket
  if((sock_desc = socket(domain, SOCK_DGRAM, protocol)) == -1){ // Might be scuffed
    perror("opening UDP socket");
    printf("socket error");
    exit(EXIT_FAILURE);
  }
  sock.sd = sock_desc; // Set socket
  
  // Set state
  sock.state = INVALID;

  return sock;

}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
  // Bind
  if(bind(socket->sd, address, address_len) == -1){
    socket->state = INVALID;
    perror("TCP bind");
    printf("TCP bind error");
    exit(EXIT_FAILURE);
  }
  // socket->state = BIND;
  //printf("Succeeded\n");

  return 0;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  /* Your code here */
  microtcp_header_t client, server;
  // if(socket->state != BIND) return -1;
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1, receivedSYNACK = 0;

  // Client SYN
  client.seq_number = htonl(100); // Should be rand
  client.ack_number = htonl(0);
  client.control = htons(1 << 1);
  sendto(socket->sd,
        (const void *)&client,
        sizeof(microtcp_header_t),
        0,
        address,
        address_len
  );

  // Server SYN ACK
  while(received < 0){
    received = recvfrom(socket->sd, (void *)&server, sizeof(microtcp_header_t),  
                0, (struct sockaddr *)address, 
                &address_len);
  }

  // If server successfully received first sequence number
  if(ntohl(server.ack_number) == ntohl(client.seq_number) + 1){

    // If server sent SYN ACK
    if(ntohs(server.control) >> 1 == 5){

      printf("Server received SYN and I received SYNACK\n");
      client.ack_number = htonl(ntohl(server.seq_number) + 1);
      client.seq_number = htonl(ntohl(server.ack_number));
    }else{
      perror("wrong packet");
      printf("Some kind of problem %d\n", ntohs(server.control) >> 1);
      return -1;
    }
  }else{
    printf("Some kind of problem %d %d\n", ntohl(server.ack_number), ntohl(client.seq_number) + 1);
    return -1;
  }

  // Client ACK
  client.control = htons(4 << 1);
  printf("ACK num %d\n", ntohl(client.ack_number));
  sendto(socket->sd, (const void *)&client, sizeof(microtcp_header_t), 
        0, address,  
            address_len); 

  if(receivedSYNACK == 1) printf("I sent my handshake\n");
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  microtcp_header_t client, server;
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1, receivedSYN = 0, receivedACK = 0;

  // Client SYN

  while(received < 0){
    received = recvfrom(socket->sd,
                        (void *)&client,
                        sizeof(microtcp_header_t),  
                        0,
                        address,
                        &address_len
    );
  }

  if(ntohs(client.control) >> 1 == 1){
    server.ack_number = htonl(ntohl(client.seq_number) + 1);
    server.seq_number = htonl(200);
  }else{
    printf("Some kind of problem\n");
    return -1;
  }

  // Server SYN ACK
  server.control = htons(5 << 1);
  printf("%d\n", ntohs(server.control));
  sendto(socket->sd, (const void *)&server, sizeof(microtcp_header_t), 
        MSG_CONFIRM, address,  
            address_len); 

  // printf("SEG3\n");


  // Client ACK
  received = -1;
  while(received < 0){
    received = recvfrom(socket->sd, (void *)&client, sizeof(microtcp_header_t),  
                0, address, 
                &address_len);

    // 100 ->  100 << 1 -> 1000 ACK. 1000 >> 1 = 100 = 4
    if(ntohs(client.control) >> 1 == 4){
      printf("Packet was ACK\n");
      receivedACK = 1;
    }
    if(ntohs(client.control) >> 1 == 1) printf("Packet was SYN");
    if(ntohs(client.control) >> 1 == 5) printf("Packet was SYNACK");
  }

  if(ntohl(client.ack_number) == ntohl(server.seq_number) + 1){
    if(ntohs(client.control) >> 1 == 4){
      printf("Handshake complete.");
    }
  }else{

    printf("%d %d\n", ntohl(client.ack_number), ntohl(server.seq_number) + 1);
  }

  //if(receivedSYN == 1 && receivedACK == 1) printf("3 way handshake done.\n");
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  /* Your code here */
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  /* Your code here */
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
