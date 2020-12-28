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
#define CLIENT 0
#define SERVER 1

microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t sock; // Socket
  int sock_desc; // Socket descriptor
  srand(time(NULL)); // Give random seed to rand
  
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

  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1;
  socket->type = CLIENT;
  //tofix
  socket->cwnd = 42342;

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  // Client SYN
  client.seq_number = htonl(rand()); // Random sequence number
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
      printf(" Inside %d\n", ntohs(server.window));
      // Get win size
      socket->init_win_size = ntohs(server.window);
      socket->curr_win_size = ntohs(server.window);
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
  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1;
  socket->type = SERVER;
  socket->init_win_size = MICROTCP_WIN_SIZE;
  socket->curr_win_size = MICROTCP_WIN_SIZE;

  //tofix
  socket->cwnd = 42342;

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

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
  if(ntohs(client.control) != SYN){
    perror("handshake failed");
    socket->state = INVALID;
    return -1;
  }

  // Init Recv Win
  printf("%d\n", socket->curr_win_size);
  server.window = htons(socket->curr_win_size);
  server.ack_number = htonl(ntohl(client.seq_number) + 1);
  server.seq_number = htonl(rand());

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
      printf("Handshake complete.\n"); // Will be gone in 2nd phase
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
  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  struct sockaddr_in address = socket->address; // Get saved addr from socket
  socklen_t address_len = socket->address_len;
  int received = -1;

  printf("I entered\n");

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  // HOST FIN, ACK
  client.seq_number = htonl(rand()); // Should be rand
  client.ack_number = htonl(0);
  client.control = htons(FINACK);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t),
    0,
    (struct sockaddr *)&address,
    address_len
  );

  // PEER ACK
  while(received < 0){
    received = recvfrom(socket->sd,
      (void *)&server,
      sizeof(microtcp_header_t),  
      0,
      (struct sockaddr *)&address, 
      &address_len
    );
  }
  
  received = -1; // Packet received

  // Check if packet is ACK and check acknowledge
  if(ntohs(server.control) == ACK){
    if(socket->type == CLIENT && ntohl(server.ack_number) == ntohl(client.seq_number) + 1){
      socket->state = CLOSING_BY_HOST;
    }
  }else{
    perror("shutdown failed");
    return 1;
  }

  // Server procedures are done here
  if(socket->type == SERVER)
    return 0;

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
    perror("shutdown failed 2");
    return 1;
  }

  // Client FIN, ACK
  client.seq_number = server.ack_number;
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

  int i, remaining = length, data_sent = 0, to_send, chunks = 0, buff_index = 0;
  void* buff = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);
  struct sockaddr_in address = socket->address; // Get saved addr from socket
  socklen_t address_len = socket->address_len;
  microtcp_header_t client, server;

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  while(data_sent < length){
    // Get chunk
    to_send = MIN(remaining, MIN(socket->cwnd, socket->curr_win_size));

    chunks = to_send / MICROTCP_MSS;
    for(i = 0; i < chunks; i++){
      printf("1");
      memset(&buff, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);

      // Get data length
      ((microtcp_header_t*)buff)->data_len = htonl(MICROTCP_MSS);

      if(data_sent == 0){
        socket->seq_number = rand();
        ((microtcp_header_t*)buff)->seq_number = htonl(socket->seq_number); // Random sequence number
        ((microtcp_header_t*)buff)->ack_number = htonl(0);
      }else{
        ((microtcp_header_t*)buff)->seq_number = socket->seq_number + i * (sizeof(microtcp_header_t) + MICROTCP_MSS);
      }

      memcpy(buff + sizeof(microtcp_header_t), buffer + i*MICROTCP_MSS, MICROTCP_MSS);

      sendto(socket->sd,
            buff,
            sizeof(microtcp_header_t) + MICROTCP_MSS,
            0,
            (struct sockaddr *)&address,
            address_len
      );

      data_sent += MICROTCP_MSS;
    }

    if(to_send % MICROTCP_MSS){
      chunks++;
      printf("%s\n", buffer + (chunks - 1) * MICROTCP_MSS);
      ((microtcp_header_t*)buff)->data_len = to_send % MICROTCP_MSS;

      if(data_sent == 0){
        socket->seq_number = rand();
        ((microtcp_header_t*)buff)->seq_number = htonl(socket->seq_number); // Random sequence number
        ((microtcp_header_t*)buff)->ack_number = htonl(0);
      }else{
        ((microtcp_header_t*)buff)->seq_number = socket->seq_number + (chunks - 1) * (sizeof(microtcp_header_t) + MICROTCP_MSS);
      }

      memcpy(buff + sizeof(microtcp_header_t), buffer + (chunks - 1) * MICROTCP_MSS, MICROTCP_MSS);

      printf("%s\n", buff + sizeof(microtcp_header_t));

      sendto(socket->sd,
            buff,
            sizeof(microtcp_header_t) + ((microtcp_header_t*)buff)->data_len,
            0,
            (struct sockaddr *)&address,
            address_len
      );

      data_sent += to_send % MICROTCP_MSS;
    }

  }

}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  struct sockaddr_in address = socket->address; // Get saved addr from socket
  socklen_t address_len = socket->address_len;
  int received = -1;

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

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

  // If client requested shutdown
  if(ntohs(client.control) == FINACK){
    printf("Was finack %d %d\n", ntohs(client.control), FINACK);
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

    // Shutdown from server
    if(microtcp_shutdown(socket,0) == 0){
      socket->state = CLOSED;
      printf("Server closed connection\n");
      return -20;
    }
  }

  return 1;
}