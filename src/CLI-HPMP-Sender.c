#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// HPMP server runs by default on port 777, you can change it if you want
#define PORT 777

int main(int argc, char *argv[]) {
  if (argc != 4 || strcmp(argv[1], "-m") != 0) {
    printf("No arguments provided!\nUsage: %s -m \"message\" <server-ip>\n", argv[0]);
    return 1;
  }
  char *message = argv[2];
  char *server_ip = argv[3];

  int sock = 0;
  struct sockaddr_in serv_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("[x] ERROR: Socket creation error");
    return 1;
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
    perror("[x] ERROR: Invalid address / Address not supported");
    return 1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("[x] ERROR: Connection Failed");
    return 1;
  }

  send(sock, message, strlen(message), 0);
  close(sock);
  return 0;
}
