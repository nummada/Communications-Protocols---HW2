#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <math.h>
#include <netinet/tcp.h> //pentru disable
#include "structs.h"
//#include "utilsf.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}


//functie care returneaza index-ul unui client
int clientIndex(struct clientEntry *clientsEntries, int nrOfClients, int i){
	for(int j = 0 ; j < nrOfClients ; j++){
		if(clientsEntries[j].sockTcp == i){
			return j;
		}
	}
	return -1;
}

//functie cu ajutorul careia server-ul trimite exit catre toti subscriberii
void exitAll(struct clientEntry *clientsEntries, int nrOfClients, char buffer[BUFLEN], int sockTcp){
	message msg;
	memset(&msg, 0, sizeof(msg));
	strcpy(msg.info, "exit");
	for(int j = 0 ; j < nrOfClients ; j++){
		int n = send(clientsEntries[j].sockTcp, &msg, sizeof(msg), 0);
		DIE(n < 0, "send");
	}
}

//functie care sterge un topic la unsubscribe
void deleteTopic(struct clientEntry *client, char* name){
	int index = 0;
	for(int i = 0 ; i < client->nrOfTopics ; i++){
		if(strcmp(client->topics[i].name, name) == 0){
			index = i;
		}
	}
	for (int j = index; j < client->nrOfTopics - 1; j++) { 
		client->topics[j] = client->topics[j + 1];
	}
	client->nrOfTopics = client->nrOfTopics - 1;
}

//functie care initializeaza un client
void initClient(struct clientEntry *clientsEntries, int nrOfClients, int newsockTcp,
	struct sockaddr_in cli_addr){

	clientsEntries[nrOfClients].sockTcp = newsockTcp;
	clientsEntries[nrOfClients].ip = cli_addr.sin_addr.s_addr;
	clientsEntries[nrOfClients].port= ntohs(cli_addr.sin_port);
	clientsEntries[nrOfClients].sin_addr = cli_addr.sin_addr;
	clientsEntries[nrOfClients].nrOfTopics = 0;
	clientsEntries[nrOfClients].topicMaxLen = 2;
	clientsEntries[nrOfClients].topics = calloc(2, sizeof(struct topic));
	clientsEntries[nrOfClients].active = 1;
	clientsEntries[nrOfClients].msgMaxLen = 2;
	clientsEntries[nrOfClients].nrOfMessages = 0;
	clientsEntries[nrOfClients].messages = calloc(2, sizeof(message));
	clientsEntries[nrOfClients].index = 0;
}

void initMessageFromUdp(udpMessage msg, message *msgToSend, char *noString){
	//INT
	if(msg.dataType == 0){
		int sign = msg.content[0];
		double no = ntohl(*(uint32_t*)(msg.content + 1));
		if(sign == 1) no *= -1;
		strcat(msgToSend->info, "INT - ");
		sprintf(noString, "%d", (int)no);
		strcat(msgToSend->info, noString);
	//SHORT_REAL
	}else if(msg.dataType == 1){
		double no = ntohs(*(uint16_t*)(msg.content));
		no /= 100;
		strcat(msgToSend->info, "SHORT_REAL - ");
		sprintf(noString, "%.2f", no);
		strcat(msgToSend->info, noString);
	//FLOAT
	}else if(msg.dataType == 2){
		int sign = msg.content[0];
		double no = ntohl(*(uint32_t*)(msg.content + 1));
		uint8_t negativePower = msg.content[1+4];
		no = no / pow(10, negativePower);
		if(sign == 1) no *= -1;
		strcat(msgToSend->info, "FLOAT - ");
		sprintf(noString, "%.4f", no);
		strcat(msgToSend->info, noString);
	//STRING
	}else if(msg.dataType == 3){
		strcat(msgToSend->info, "STRING - ");
		strcat(msgToSend->info, msg.content);
	}
	strcat(msgToSend->info, "\0");
}

//functie care printeaza un mesaj atunci cand un client se conecteaza
void printMessageForNewClient(struct clientEntry *clientsEntries, int index){
	printf("New client (%s) ", clientsEntries[index].id);
	printf("connected from %s", inet_ntoa(clientsEntries[index].sin_addr));
	printf(":%d.\n", clientsEntries[index].port);
}

/*functie folosita atunci cand un client da subscribe
se adauga un topic in lista de topicuri, daca nu exist
se actualizeaza daca exista*/
void subscribeClient(struct clientEntry *clientsEntries, int index, char buffer[BUFLEN]){
	int verify = 1;
	if(index != -1){
		struct topic topic;
		topic.name = strtok(NULL, " ");
		topic.SF = atoi(strtok(NULL, " "));

		//daca am depasit dimensiunea maxima in lista de topic-uri
		if(clientsEntries[index].nrOfTopics == clientsEntries[index].topicMaxLen){
			clientsEntries[index].topics = realloc(clientsEntries[index].topics,
				(clientsEntries[index].topicMaxLen *= 2) * sizeof(struct topic));
		}

		//daca topic-ul deja exista, se updateaza SF-ul
		for(int k = 0 ; k < clientsEntries[index].nrOfTopics ; k++){
			if(strcmp(topic.name, clientsEntries[index].topics[k].name) == 0){
				memset(buffer, 0, BUFLEN);
				clientsEntries[index].topics[k].SF = topic.SF;
				verify = 0;
				message msg;
				memset(&msg, 0, sizeof(msg));
				sprintf(msg.info, "Topic <%s> already exists, so it has been updated.",
					clientsEntries[index].topics[k].name);
				int n= send(clientsEntries[index].sockTcp, &msg, sizeof(msg), 0);
				DIE(n < 0, "send");
			}
		}
		//se adauga topic-ul daca el nu exista deja
		if(verify == 1){
			clientsEntries[index].topics[clientsEntries[index].nrOfTopics++] = topic;
		}
	}
}

//functie care salveaza mesajele pentru clientii inactivi
void saveMessagesForClients(struct clientEntry *clientsEntries, int j,
	udpMessage msg, message msgToSend){

	for(int k = 0 ; k < clientsEntries[j].nrOfTopics ; k++){
		if(strcmp(clientsEntries[j].topics[k].name, msg.topic) == 0){
			//daca clientul este abonat cu SF=1 pentru topic
			if(clientsEntries[j].topics[k].SF == 1){
				clientsEntries[j].messages[clientsEntries[j].nrOfMessages] = msgToSend;
				clientsEntries[j].nrOfMessages++;									
			}
		}
	}
}

/*functie care afiseaza un mesaj de eroare cand se conecteaza un client cu acelasi id ca unul deja conectat*/
void printErrorConnectionMessage(struct clientEntry *clientsEntries, int nrOfClients){
	message msg;
	memset(&msg, 0, sizeof(msg));
	strcpy(msg.info, "Id already in use.");
	int n = send(clientsEntries[nrOfClients].sockTcp, &msg, sizeof(msg), 0);
	DIE(n < 0, "send");
	memset(&msg, 0, sizeof(msg));
	strcpy(msg.info, "exit");
	n = send(clientsEntries[nrOfClients].sockTcp, &msg, sizeof(msg), 0);
	DIE(n < 0, "send");
}

//functie care trimite unui client actualizare a unui topic
void sendTopicUpdate(struct clientEntry *clientsEntries, int j, udpMessage msg, message msgToSend){
	for(int k = 0 ; k < clientsEntries[j].nrOfTopics ; k++){
		if(strcmp(clientsEntries[j].topics[k].name, msg.topic) == 0){
			message aux;
			memset(&aux, 0, sizeof(message));
			strcpy(aux.info, msgToSend.info);
			int n = send(clientsEntries[j].sockTcp, &aux, sizeof(msg), 0);
			DIE(n < 0, "send");
		}
	}
}

//functie care primeste un mesaj de tip udp, parseaza mesajul si il trimite la clienti sau il salveaza
void recvUdpMessage(int* nrOfClients, int sockUdp, struct sockaddr_in serv_addr_udp,
	struct clientEntry *clientsEntries){

	socklen_t size = sizeof(serv_addr_udp);
	udpMessage msg;
	memset(&msg, 0, sizeof(udpMessage));
	int n = recvfrom(sockUdp, &msg, sizeof(msg), 0, ((struct sockaddr*)&serv_addr_udp), &size);
	DIE(n == 0, "recv");

	message msgToSend;
	//creez mesajul primit de la clientul udp care urmeaza sa fie trimis catre client
	memset(msgToSend.info, 0, strlen(msgToSend.info));
	strcpy(msgToSend.info, "infotopic");
	char port [10];
	sprintf(port, "%d", ntohs(serv_addr_udp.sin_port));
	strcat(msgToSend.info, inet_ntoa(serv_addr_udp.sin_addr));
	strcat(msgToSend.info, ":");
	strcat(msgToSend.info, port);
	strcat(msgToSend.info, " - ");
	strcat(msgToSend.info, msg.topic);
	strcat(msgToSend.info, " - ");

	//string auxiliar in care salvez numere
	char noString [20];

	initMessageFromUdp(msg, &msgToSend, noString);

	for(int j = 0 ; j < *nrOfClients ; j++){
		//daca subscriberul este activ
		if(clientsEntries[j].active == 1){
			sendTopicUpdate(clientsEntries, j, msg, msgToSend);
		//daca este deconectat
		}else{
		//daca am depasit dimensiunea maxima in lista de mesaje
			if(clientsEntries[j].nrOfMessages == clientsEntries[j].msgMaxLen){
				clientsEntries[j].messages = realloc(clientsEntries[j].messages,
					clientsEntries[j].msgMaxLen*2*sizeof(message));
				clientsEntries[j].msgMaxLen *= 2;
			}
		//salvez mesajele pentru clientii deconectati in lista de mesaje
			saveMessagesForClients(clientsEntries, j, msg, msgToSend);
		}
	}
}

int main(int argc, char *argv[]) {
	int sockTcp, sockUdp,  newsockTcp, portno;
	char buffer[BUFLEN];
	char *auxMessage, *command;
	struct sockaddr_in serv_addr_tcp, cli_addr, serv_addr_udp;
	int n, i, retTcp, retUdp;
	socklen_t clilen;
	struct clientEntry *clientsEntries = calloc(2, sizeof(struct clientEntry));
	int maxNrOfClients = 2;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	//creeze socket pentru tcp, respectiv udp
	sockTcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockTcp < 0, "socket");
	sockUdp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockTcp < 0, "socket");


	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	//setez structura de tip sockaddr_in pentru tcp
	memset((char *) &serv_addr_tcp, 0, sizeof(serv_addr_tcp));
	serv_addr_tcp.sin_family = AF_INET;
	serv_addr_tcp.sin_port = htons(portno);
	serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;
	//setez structura de tip sockaddr_in pentru udp
	memset((char *) &serv_addr_udp, 0, sizeof(serv_addr_udp));
	serv_addr_udp.sin_family = AF_INET;
	serv_addr_udp.sin_port = htons(portno);
	serv_addr_udp.sin_addr.s_addr = INADDR_ANY;

	//bind pentru socket-ul tcp
	retTcp = bind(sockTcp, (struct sockaddr *) &serv_addr_tcp, sizeof(struct sockaddr));
	DIE(retTcp < 0, "bind");
	//bind pentru socket-ul udp
	retUdp = bind(sockUdp, (struct sockaddr *) &serv_addr_udp, sizeof(struct sockaddr));
	DIE(retUdp < 0, "bind");

	//listen pe socketul tcp
	retTcp = listen(sockTcp, MAX_CLIENTS);
	DIE(retTcp < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	//se adauga fd pentru tcp
	FD_SET(sockTcp, &read_fds);
	//se adauga fd pentru udp
	FD_SET(sockUdp, &read_fds);
	//se
	FD_SET(0, &read_fds);
	fdmax = sockUdp;

	int nrOfClients= 0;


	while (1) {
		tmp_fds = read_fds; 
		retTcp = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if( retTcp  < 0 ) {
			perror( "select" );
			return 1;
		}
		DIE(retTcp < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockTcp) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockTcp = accept(sockTcp, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockTcp < 0, "accept");
					int NON_ZERO_VALUE = 9;
					int disable = setsockopt(newsockTcp, IPPROTO_TCP, TCP_NODELAY,
						(char *)&(NON_ZERO_VALUE), sizeof(int));
					DIE(disable != 0, "disable");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockTcp, &read_fds);
					if (newsockTcp > fdmax) {
						fdmax = newsockTcp;
					}
					//daca s-a atins capacitatea maxima
					if(nrOfClients == maxNrOfClients){
						clientsEntries =
							realloc(clientsEntries, maxNrOfClients*2*sizeof(struct clientEntry));
						maxNrOfClients *= 2;
					}

					//initializare client
					initClient(clientsEntries, nrOfClients, newsockTcp, cli_addr);
					nrOfClients++;

				//mesaj la stdin
				}else if(i == 0){
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN, stdin);
						//daca primesc exit, trebuie sa deconectez toti clientii inainte
					if(strcmp(buffer, "exit")){
						exitAll(clientsEntries, nrOfClients, buffer, sockTcp);
						close(sockTcp);
						return 0;
					}
				//mesaj pe socketul udp
				}else if(i == sockUdp){
					recvUdpMessage(&nrOfClients, sockUdp, serv_addr_udp, clientsEntries);
				/* s-au primit date pe unul din socketii de client,
				 asa ca serverul trebuie sa le receptioneze*/
				} else {
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					// conexiunea s-a inchis
					if (n == 0) {
						int index = clientIndex(clientsEntries, nrOfClients, i);
						if(index != -1){
							printf("Client (%s) disconnected.\n", clientsEntries[index].id);
							clientsEntries[index].active = 0;
						}
						close(i);

						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					//s-au primit date
					} else {
						auxMessage = calloc(strlen(buffer) + 1, sizeof(char));
						memcpy(auxMessage, buffer, strlen(buffer));
						command = strtok(auxMessage, " ");
						int index = clientIndex(clientsEntries, nrOfClients, i);
						//daca se primeste informatii despre id
						if(strcmp(command, "id:") == 0){
							//daca este index valid
							if(index != -1){
								strcpy(clientsEntries[index].id, strtok(NULL, " "));
								int verifyConnection = 1;
								for(int j = 0 ; j < nrOfClients - 1 ; j++){
									if(strcmp(clientsEntries[j].id, clientsEntries[index].id) == 0){
										//daca exista un client deja conectat cu acelasi id
										if(clientsEntries[j].active == 1){
											verifyConnection = 0;
											break;
										}else{
											//clientul se reconecteaza, se trimit mesajele pierdute pt SF = 1
											for(int k = 0 ; k < clientsEntries[j].nrOfMessages ; k++){
												n = send(clientsEntries[j].sockTcp,
													&(clientsEntries[j].messages[k]),
													sizeof(message), 0);
												DIE(n < 0, "send");
												clientsEntries[j].index++;
											}
											//se reinitializeaza lista de mesaje
											//free
											free(clientsEntries[j].messages);
											clientsEntries[j].msgMaxLen = 2;
											clientsEntries[j].nrOfMessages = 0;
											clientsEntries[j].messages =
												calloc(2, sizeof(message));
											clientsEntries[j].active = 1;
										}
									}
								}
								//client nou
								if(verifyConnection == 1){
									printMessageForNewClient(clientsEntries, index);
								//clientul deja exista conectat
								}else{
									nrOfClients--;
									printErrorConnectionMessage(clientsEntries, nrOfClients);
								}
							}
						//daca se primesc informatii despre subscribe
						}else if(strcmp(command, "subscribe") == 0){
							subscribeClient(clientsEntries, index, buffer);
						//daca se primesc informatii despre unsubscribe
						}else if(strcmp(command, "unsubscribe") == 0){
							if(index != -1){
								char* name = strtok(NULL, " \n");
								deleteTopic(clientsEntries, name);
								free(name);
							}
						}
						free(auxMessage);
					}
				}
			}
		}
	}

	for(int i = 0 ; i < maxNrOfClients ; i++){
		free(clientsEntries[i].messages);
		for(int j = 0 ; j < clientsEntries[i].topicMaxLen ; j++){
			free(clientsEntries[i].topics[j].name);
		}
		free(clientsEntries[i].topics);
	}
	free(clientsEntries);
	free(command);
	close(sockTcp);
	close(sockUdp);
	close(0);
	return 0;
}
