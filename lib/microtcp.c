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
  microtcp_sock_t s; // Socket
  int sock_desc; // Socket descriptor
  srand(time(NULL)); // Give random seed to rand
  
  // Create socket
  if((sock_desc = socket(domain, SOCK_DGRAM, protocol)) == -1){ // Might be scuffed
    perror("opening UDP socket");
    printf("socket error");
    exit(EXIT_FAILURE);
  }

  // Initialize socket values
  s.sd = sock_desc;
  s.packets_lost = 0;
  s.packets_received = 0;
  s.packets_send = 0;
  s.bytes_lost = 0;
  s.bytes_received = 0;
  s.bytes_send = 0;

  // Set timeout
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
  setsockopt(s.sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

  return s;
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

  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  // Update socket
  socket->type = CLIENT;
  socket->cwnd = MICROTCP_INIT_CWND;
  socket->ssthresh = MICROTCP_INIT_SSTHRESH;
  socket->seq_number = rand();
  socket->ack_number = 0;

  // Client SYN
  client.seq_number = htonl(socket->seq_number); // Random sequence number
  client.ack_number = htonl(socket->ack_number);
  client.control = htons(SYN);
  sendto(socket->sd,
    (const void *)&client,
    sizeof(microtcp_header_t),
    0,
    address,
    address_len
  );
  socket->packets_send++;
  socket->bytes_send += sizeof(microtcp_header_t);

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
      socket->seq_number = ntohl(server.ack_number);
      //socket->ack_number = ntohl(server.seq_number) + received;
      socket->ack_number = ntohl(server.seq_number) + 1;
      socket->init_win_size = ntohs(server.window);
      socket->curr_win_size = ntohs(server.window);

      client.ack_number = htonl(socket->ack_number);
      client.seq_number = htonl(socket->seq_number);
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
  socket->packets_send++;
  socket->bytes_send += sizeof(microtcp_header_t);

  // Set state
  socket->state = ESTABLISHED;
  if(DEBUG) printf("CLIENT - INIT_WIN = %d CURR_WIN = %d\n", socket->init_win_size, socket->curr_win_size);
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  client.data_len = htonl(32);
  int received = -1;

  // Init server's socket
  socket->type = SERVER;
  socket->init_win_size = MICROTCP_WIN_SIZE;
  socket->curr_win_size = MICROTCP_WIN_SIZE;
  socket->recvbuf = malloc(MICROTCP_RECVBUF_LEN);
  socket->buf_fill_level = 0;

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
  if(DEBUG) printf("%d\n", socket->curr_win_size);
  server.window = htons(socket->curr_win_size);
  //server.ack_number = htonl(ntohl(client.seq_number) + 1);
  socket->ack_number = ntohl(client.seq_number) + 1;
  //socket->ack_number = ntohl(client.seq_number) + received;
  socket->seq_number = rand();
  server.seq_number = htonl(socket->seq_number);
  server.ack_number = htonl(socket->ack_number);

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
      if(DEBUG) printf("Handshake complete.\n"); // Will be gone in 2nd phase
      socket->seq_number = ntohl(client.ack_number);
      socket->state = ESTABLISHED;
    }else{
      socket->state = INVALID;
      if(DEBUG) printf("Handshake failed.\n");
    }
  }else{
    socket->state = INVALID;
    if(DEBUG) printf("Handshake faile.\n");
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
    if(DEBUG) printf("I requested a connection shutdown\n");
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

  if(DEBUG) printf("Connection shutdown\n");
  socket->state = CLOSED;
}

/*
 * Return maximum packet length
*/
static inline
int
getMaxPacketSize(int remaining, int cwnd, int win)
{
  if(remaining < cwnd && remaining < win)
    return remaining;
  else if(cwnd < win)
    return cwnd;
  else
    return win;
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{

  int i, rec = 0, remaining = length, data_sent = 0, to_send, chunks = 0, bytes_lost, last_ack = 0, dup_acks = 0;
  void* buff = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS), *recbuff = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);
  struct sockaddr_in address = socket->address; // Get saved addr from socket
  socklen_t address_len = socket->address_len;
  microtcp_header_t client, server;

  if(DEBUG) printf("sent: %d, length: %d\n", data_sent, length);
  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  while(data_sent < length){

    // Get chunk size
    to_send = getMaxPacketSize(remaining, socket->cwnd, 55555);
    if(DEBUG) printf("I picked %d | REMAINING = %d CWND = %d SSTHRESH = %d WINDOWS = %d\n", to_send, remaining, socket->cwnd, socket->ssthresh, socket->curr_win_size);
    //printf("Remaining: %d\n", remaining);
    //printf("Sending a segment of %d\n", to_send);

    chunks = to_send / MICROTCP_MSS;
    for(i = 0; i < chunks; i++){
      memset(buff, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);
      if(DEBUG) printf("Sending chunk of 1400 number %d\n", i);

      // Get data length and seq num
      ((microtcp_header_t*)buff)->data_len = htonl(MICROTCP_MSS);
      ((microtcp_header_t*)buff)->seq_number = htonl(socket->seq_number + i * MICROTCP_MSS);

      // Put current buffer part in packet
      memcpy(buff + sizeof(microtcp_header_t), buffer + data_sent, MICROTCP_MSS);

      // Checksum
      ((microtcp_header_t*)buff)->checksum = htonl(crc32(buff, sizeof(microtcp_header_t) + MICROTCP_MSS));
      if(DEBUG) printf("Checksum %lu\n", ntohl(((microtcp_header_t*)buff)->checksum));

      sendto(socket->sd,
            buff,
            sizeof(microtcp_header_t) + MICROTCP_MSS,
            0,
            (struct sockaddr *)&address,
            address_len
      );

      data_sent += MICROTCP_MSS;
      remaining = length - data_sent;
    }

    // If there is any remaining thats not 1400B
    if(to_send % MICROTCP_MSS != 0){
      chunks++;
      memset(buff, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);
      if(DEBUG) printf("Sending remaining %d bytes\n", to_send % MICROTCP_MSS);

      // Get data length and seq num
      ((microtcp_header_t*)buff)->data_len = htonl(to_send % MICROTCP_MSS);
      ((microtcp_header_t*)buff)->seq_number = htonl(socket->seq_number + i * MICROTCP_MSS);

      // Put current buffer part in packet
      memcpy(buff + sizeof(microtcp_header_t), buffer + data_sent, MICROTCP_MSS);

      // Checksum
      ((microtcp_header_t*)buff)->checksum = htonl(crc32(buff, sizeof(microtcp_header_t) + to_send % MICROTCP_MSS));
      if(DEBUG) printf("Checksum %lu\n", ntohl(((microtcp_header_t*)buff)->checksum));
      
      sendto(socket->sd,
            buff,
            sizeof(microtcp_header_t) + ntohl(((microtcp_header_t*)buff)->data_len),
            0,
            (struct sockaddr *)&address,
            address_len
      );

      data_sent += to_send % MICROTCP_MSS;
    }

    // Get ACKS
    for(int i = 0; i < chunks; i++){
      // Receive
      rec = recvfrom(socket->sd, &server, sizeof(microtcp_header_t), 0, &(socket->address), &(socket->address_len));
      if(rec < 0){

        // Timeout
        socket->ssthresh = socket->cwnd / 2;
        socket->cwnd = MICROTCP_MSS;

        // Retransmit
        bytes_lost = socket->seq_number - ntohl(server.ack_number);
        if(DEBUG) printf("RETRANSMITTING %d LOST BYTES\n", bytes_lost);
        socket->seq_number = ntohl(server.ack_number);
        socket->curr_win_size = ntohl(server.window);
        data_sent -= bytes_lost;
      }


      // Check if ACK
      if(ntohs(server.control) != ACK)
        continue;

      if(ntohl(server.ack_number) > socket->seq_number){ // Normal
        // Advance seq nums
        socket->seq_number = ntohl(server.ack_number);
        socket->curr_win_size = ntohl(server.window);
        last_ack = ntohl(server.ack_number);
        dup_acks = 0;
      }else if(dup_acks >= 3){ // Fast Retransmit
        // GO-BACK-N
        bytes_lost = socket->seq_number - ntohl(server.ack_number);
        if(DEBUG) printf("RETRANSMITTING %d LOST BYTES\n", bytes_lost);
        socket->seq_number = ntohl(server.ack_number);
        socket->curr_win_size = ntohl(server.window);
        data_sent -= bytes_lost;
      }else if(ntohl(server.ack_number) <= socket->seq_number + MICROTCP_MSS && last_ack == ntohl(server.ack_number)){ // Duplicate ACK
        dup_acks++;
      }

      // Congestion Control
      if(socket->cwnd <= socket->ssthresh){ // Slow Start
        socket->cwnd += MICROTCP_MSS;
      }else if(socket->cwnd > socket->ssthresh){ // Congestion Avoidance
        socket->cwnd += 1;
      }

      // Flow Control
      if(socket->curr_win_size == 0){
        // If window is 0, we will send empty packets till its normal again
        
        // Send 0 data packet
        sendto(socket->sd,
            buff,
            sizeof(microtcp_header_t),
            0,
            (struct sockaddr *)&address,
            address_len
        );

        usleep(rand() % MICROTCP_ACK_TIMEOUT_US);
      }
    }
  }

  free(buff);
  free(recbuff);
  return data_sent;
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  microtcp_header_t client, server; // Headers
  client.window = htons(MICROTCP_WIN_SIZE);
  struct sockaddr_in address = socket->address; // Get saved addr from socket
  socklen_t address_len = socket->address_len;
  int received = -1, remaining_bytes = length, received_total = 0, checksum, buffer_index = 0;
  uint8_t *recvbuf = socket->recvbuf;
  uint8_t *buf = (uint8_t*)malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);

  // If connection is shutdown, exit with -1
  if(socket->state == CLOSED) return -1;

  if(DEBUG) printf("ACK %d\n", socket->ack_number);
  memset(&client, 0, sizeof(microtcp_header_t));
  memset(&server, 0, sizeof(microtcp_header_t));

  // Receive a packet
  while(remaining_bytes > 0){
    memset(buf, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);

    // Receive
    received = recvfrom(socket->sd,
      buf,
      sizeof(microtcp_header_t) + MICROTCP_MSS,  
      0,
      (struct sockaddr *)&address, 
      &address_len
    );
    if(received < 0){
      continue;
    }

    memset(&server, 0, sizeof(microtcp_header_t));

    if(DEBUG) printf("Expected: %d Received: %d\n", socket->ack_number, ntohl(((microtcp_header_t*)buf)->seq_number));

    /* ----------- SHUTDOWN CHECK ------------- */

    // If client requested shutdown
    if(ntohs(((microtcp_header_t*)buf)->control) == FINACK){
      if(DEBUG) printf("Was finack %d %d\n", ntohs(((microtcp_header_t*)buf)->control), FINACK);
      // Acknowledge the FIN with an ACk
      server.ack_number = htonl(ntohl(((microtcp_header_t*)buf)->seq_number) + 1);
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
        if(DEBUG) printf("Server closed connection\n");

        // Empty receive buffer
        memcpy(buffer, recvbuf, socket->buf_fill_level);
        socket->buf_fill_level = 0;
        socket->curr_win_size = socket->init_win_size;

        return received_total;
      }
    }

    /* ----------- CHECKS ------------- */
    if(DEBUG) printf("Received :%d\n", received);

    checksum = ntohl(((microtcp_header_t*)buf)->checksum);
    ((microtcp_header_t*)buf)->checksum = 0;
    if(checksum != crc32(buf, received) || ntohl(((microtcp_header_t*)buf)->seq_number) != socket->ack_number){
      
      // Send duplicate ACK
      memset(&server, 0, sizeof(microtcp_header_t));
      server.ack_number = htonl(socket->ack_number);

      sendto(socket->sd, &server, sizeof(microtcp_header_t), 0, (struct sockaddr *)&address, address_len);
      socket->packets_send++;
      socket->bytes_send += sizeof(microtcp_header_t);

      socket->packets_lost++;
      socket->bytes_lost += received;
      continue;
    }

    // ACK -<<>>LP>LPPL{ASCSCACS}
    //if(ntohl(((microtcp_header_t*)buf)->seq_number) == socket->ack_number) socket->ack_number = socket->ack_number + received - sizeof(microtcp_header_t);

    /* ----------- CORRECT PACKET ------------- */
    socket->ack_number = socket->ack_number + received - sizeof(microtcp_header_t); // <--
    socket->bytes_received += received;
    socket->packets_received++;

    // Decrease win size
    socket->curr_win_size = socket->curr_win_size - received - sizeof(microtcp_header_t);
    if((int)(socket->curr_win_size) - (int)ntohl(((microtcp_header_t*)buf)->data_len) < 0)
      socket->curr_win_size = 0;
    //printf("Curr win size: %zu\n", socket->curr_win_size);

    // memcpy(buffer + received_total, buf + sizeof(microtcp_header_t), received - sizeof(microtcp_header_t));
    received_total += received - sizeof(microtcp_header_t);
    remaining_bytes -= received - sizeof(microtcp_header_t);
    //printf("Remaining: %d Received: %d\n", remaining_bytes, received);
    //printf("%s\n", buffer);

    // Check if recvbuf needs emptying
    if(socket->buf_fill_level > 0.85 * MICROTCP_RECVBUF_LEN){
      
      // Empty receive buffer
      memcpy(buffer + buffer_index, recvbuf, socket->buf_fill_level);
      buffer_index += socket->buf_fill_level;
      socket->buf_fill_level = 0;
      socket->curr_win_size = socket->init_win_size;
    }

    // Sliding window
    memcpy(recvbuf + buffer_index + socket->buf_fill_level, buf + sizeof(microtcp_header_t), received - sizeof(microtcp_header_t));
    socket->buf_fill_level += received - sizeof(microtcp_header_t);

    // Ready header
    server.control = htons(ACK);
    server.ack_number = htonl(socket->ack_number);
    server.window = htons(socket->curr_win_size);

    sendto(socket->sd, &server, sizeof(microtcp_header_t), 0, (struct sockaddr *)&address, address_len);
    socket->packets_send++;
    socket->bytes_send += sizeof(microtcp_header_t);
    if(DEBUG) printf("%d\n", buffer_index);
  }

  // Empty receive buffer
  memcpy(buffer + buffer_index, recvbuf, socket->buf_fill_level);
  buffer_index += socket->buf_fill_level;
  socket->buf_fill_level = 0;
  socket->curr_win_size = socket->init_win_size;

  free(buf);
  return received_total;
}