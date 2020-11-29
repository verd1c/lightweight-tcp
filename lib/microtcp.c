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
  socket->state = LISTEN;

  return 0;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  microtcp_header_t client, server;
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1;

  // Client SYN
  client.seq_number = htonl(100); // Should be rand
  client.ack_number = htonl(0);
  client.control = htons(SYN);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t),
    0,
    address,
    address_len
  );

  // Server SYN ACK
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&server,
      sizeof(microtcp_header_t),  
      0,
      (struct sockaddr *)address, 
      &address_len
    );
  }

  // If server successfully received first sequence number
  if(ntohl(server.ack_number) == ntohl(client.seq_number) + 1){

    // If server sent SYN ACK
    if(ntohs(server.control) == SYNACK){

      //printf("Server received SYN and I received SYNACK\n");
      client.ack_number = htonl(ntohl(server.seq_number) + 1);
      client.seq_number = htonl(ntohl(server.ack_number));
    }else{
      perror("handshake failed");
      socket->state = INVALID;
      return -1;
    }
  }else{
    perror("handshake failed");
    socket->state = INVALID;
    return -1;
  }

  // Client ACK
  client.control = htons(ACK);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t), 
    0,
    address,  
    address_len
  );

  socket->state = ESTABLISHED;
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

  // Check if packet was SYN and acknowledge num
  if(ntohs(client.control) == SYN){
    server.ack_number = htonl(ntohl(client.seq_number) + 1);
    server.seq_number = htonl(200);
  }else{
    perror("handshake failed");
    socket->state = INVALID;
    return -1;
  }

  // Server SYN ACK
  server.control = htons(SYNACK);
  sendto(socket->sd,
    (const void *)&server,
    sizeof(microtcp_header_t), 
    0,
    address,  
    address_len
  ); 

  // Client ACK
  received = -1;
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&client,
      sizeof(microtcp_header_t),  
      0,
      address, 
      &address_len
    );
  }

  // Check if packet was ACK and acknowledge num
  if(ntohl(client.ack_number) == ntohl(server.seq_number) + 1){
    if(ntohs(client.control) == ACK){
      printf("Handshake complete.\n");
      socket->state = ESTABLISHED;
    }else{
      socket->state = INVALID;
      printf("Handshake failed.\n");
    }
  }else{
    socket->state = INVALID;
    printf("Handshake faile.\n");
  }
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  /* Your code here */
  microtcp_header_t client, server;
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  struct sockaddr_in address = socket->address;
  socklen_t address_len = socket->address_len;
  int received = -1;

  // Client FIN, ACK
  client.seq_number = htonl(500); // Should be rand
  client.ack_number = htonl(0);
  client.control = htons(FINACK);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t),
    0,
    (struct sockaddr *)&address,
    address_len
  );

  // Wait for server's ACK
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&server,
      sizeof(microtcp_header_t),  
      0,
      (struct sockaddr *)&address, 
      &address_len
    );
  }
  received = -1; // Reset received

  // Cehck if packet is ACK and check acknowledge
  if(ntohs(server.control) == ACK && ntohl(server.ack_number) == ntohl(client.seq_number) + 1){
    socket->state = CLOSING_BY_HOST;
  }else{
    perror("shutdown failed");
    return 1;
  }

  // Wait for server's FIN ACK
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&server,
      sizeof(microtcp_header_t),  
      0,
      (struct sockaddr *)&address, 
      &address_len
    );
  }

  // Check if packet was FINACK 
  if(ntohs(server.control) == FINACK){
    printf("I requested a connection shutdown\n");
  }else{
    perror("shutdown failed");
    return 1;
  }

  // Client FIN, ACK
  client.seq_number = server.ack_number; // Should be rand
  client.ack_number = htonl(ntohl(server.seq_number) + 1);
  client.control = htons(ACK);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t),
    0,
    (struct sockaddr *)&address,
    address_len
  );

  printf("Connection shutdown\n");
  socket->state = CLOSED;
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
  microtcp_header_t client, server;
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  struct sockaddr_in address = socket->address;
  socklen_t address_len = socket->address_len;
  int received = -1;

  // Receive a packet
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&client,
      sizeof(microtcp_header_t),  
      0,
      (struct sockaddr *)&address, 
      &address_len
    );
  }

  if(ntohs(client.control) == FINACK){
    // Acknowledge the FIN with an ACk
    server.ack_number = htonl(ntohl(client.seq_number) + 1);
    server.control = htons(ACK);
    sendto(socket->sd,
      (const void *)&server,
      sizeof(microtcp_header_t),
      0,
      (struct sockaddr *)&address,
      address_len
    );
    socket->state = CLOSING_BY_PEER;

    // Finished undone work, shutdown from host, send FIN ACK
    server.seq_number = htonl(1000);
    server.control = htons(FINACK);
    sendto(socket->sd,
      (const void *)&server,
      sizeof(microtcp_header_t),
      0,
      (struct sockaddr *)&address,
      address_len
    );

    // Wait for ACK
    received = -1;
    while(received < 0){
      received = recvfrom(socket->sd,
        (void *)&client,
        sizeof(microtcp_header_t),  
        0,
        (struct sockaddr *)&address, 
        &address_len
      );
    }

    // Check if packet was ACK and acknowledge num
    if(ntohs(client.control) == ACK && ntohl(client.ack_number) == ntohl(server.seq_number) + 1){
      socket->state = CLOSED;
      printf("Server closed connection\n");
      return -20;
    }else{
      perror("shutdown failed");
    }
  }

  return 1;
}