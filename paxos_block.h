#ifndef PAXOS_BLOCK_H
#define PAXOS_BLOCK_H

#include <stdio.h> //printf, perror
#include <string> //std::string
#include <string.h> //strcpy, strlen
#include <cstring> //for memset
#include <sstream> //iss

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

#include <vector> //keep track of majorities

// PAXOS Variables
int id;
int ballot_num[2] = {0,0};
int accept_num[2] = {0,0};
int accept_val = 0;
int prop_val = 0;
std::vector<int> log; //log.push_back

// Global Majority Variables
int ack[100][5];
int accepts[100][5];

// Network Variables
std::string servers[5];
int ports[5];
struct sockaddr_in servaddrs[5];
int socks[5];

// Threading Variables (for anyone to start a thread)
int num_threads = 100;
pthread_t* threads;
int thread_i = 0;


// Functions
//void* command_handler(void*); // char*
void* message_handler(void*); // char*
void* udp_server(void*);
void serversetup();
void broadcast(char*);


#endif