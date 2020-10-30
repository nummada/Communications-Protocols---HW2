#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include "structs.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");


	fd_set read_fds;
	FD_ZERO(&read_fds);

	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);



	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");


	char id[4 + sizeof(argv[1])] = "id: ";
	strcat(id, argv[1]);


	// se trimite mesaj la server
	n = send(sockfd, id, 14, 0);
	DIE(n < 0, "send");

	while (1) {
  		// se citeste de la tastatura
		FD_SET(sockfd, &read_fds);
		FD_SET(0, &read_fds);
		int sel = select(sockfd + 1, &read_fds, NULL, NULL, NULL);

		if(FD_ISSET(0, &read_fds)){
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}else{
				char* message = calloc(strlen(buffer), sizeof(char));
				memcpy(message, buffer, strlen(buffer));
				char* command = strtok(message, " \n");
				
				if(command == NULL){
					printf("<Empty> command is not a valid command.\n");
				}else if(strcmp(command, "subscribe") == 0){
					int verifyCommand = 1;
					command = strtok(NULL, " \n");
					if(command == NULL) verifyCommand = 0;
					command = strtok(NULL, " \n");
					if(command == NULL) verifyCommand = 0;
					if(command != NULL && (atoi(command) != 0 || strlen(command) != 1) && atoi(command) != 1){
						verifyCommand = 2;
						printf("SF should be only 0 or 1.\n");
					}

					command = strtok(NULL, " \n");
					if(command != NULL) {
						verifyCommand = 0;
					}
					if(verifyCommand == 1){
						n = send(sockfd, buffer, strlen(buffer), 0);
						DIE(n < 0, "send");
						if(n >= 0){
							printf("Subscribed topic.\n");
						}
					}else if(verifyCommand == 0){
						printf("Invalid number of arguments. Arguments requested for subscribe command: topic and SF. Please introduce a valid command.\n");
					}
				}else if(strcmp(command, "unsubscribe") == 0){
					int verifyCommand = 1;
					command = strtok(NULL, " \n");
					if(command == NULL) {
						verifyCommand = 0;
						printf("Invalid number of arguments.\n");
					}
					command = strtok(NULL, " \n");
					if(command != NULL) {
						verifyCommand = 0;
						printf("Invalid number of arguments.\n");
					}
					if(verifyCommand == 1){
						n = send(sockfd, buffer, strlen(buffer), 0);
						DIE(n < 0, "send");
						if(n >= 0){
							printf("Unsubscribed topic.\n");
						}
					}
				}
			}
		}else { 
			memset(buffer, 0, BUFLEN);
			message msg;
			memset(&msg, 0, sizeof(msg));
			
			int n = recv(sockfd, &msg, sizeof(msg), 0);
			DIE(n == 0, "recv");
			if (strncmp(msg.info, "exit", 4) == 0) {
				break;
			}else if(strncmp(msg.info, "infotopic", 9) == 0){
				printf("%s\n", msg.info + 9);
			}else if (strncmp(msg.info, "Id already in use.", 18) == 0){
				printf("%s Connection refused.\n", msg.info);
			}else{
				printf("%s\n", msg.info);
			}
		}
	}

	close(sockfd);

	return 0;
}
