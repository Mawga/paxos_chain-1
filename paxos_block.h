#ifndef PAXOS_BLOCK_H
#define PAXOS_BLOCK_H

#include <stdio.h> //printf, perror
#include <string> //std::string
#include <string.h> //strcpy, strlen
#include <cstring> //for memset

#include <fstream> //file input

#include <pthread.h> //threads
#include <mutex> //mutual exclusion

#include <iostream> //cin, cout, cerr
#include <stdlib.h> //atoi
#include <unistd.h> //usleep

#include <cstdint> // for intptr_t
#include <netinet/in.h> //inet stuff
#include <arpa/inet.h> //inet stuff
#include <sys/types.h> //special types pther_t
#include <sys/socket.h> //sockets


int id;

std::string servers[5];
int ports[5];

struct sockaddr_in servaddrs[5];
int socks[5];



#endif
