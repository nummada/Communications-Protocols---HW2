#ifndef STRUCTS
#include <netinet/in.h>

typedef struct  __attribute__ ((packed)) {
	char info [2000];
} message;

typedef struct  __attribute__ ((packed)) {
	char topic [50];
	char dataType;
	char content [1500];
} udpMessage;

//structura care reprezinta un client
struct clientEntry{
	char id[11];
	int ip;
	int sockTcp;
	int port;
	struct in_addr sin_addr;
	struct topic *topics;
	int topicMaxLen;
	int nrOfTopics;
	int active;

	message *messages;
	int msgMaxLen;
	int nrOfMessages;
	int index;
}clientEntry;

//structura unui topic
struct topic{
	char* name;
	int SF;
}Topic;

#endif
