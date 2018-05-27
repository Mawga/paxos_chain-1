#include "paxos_block.h"

/* MESSAGE HANDLER THREAD:
 * Handle any message received from UDP/TCP servers and respond accordingly
 */
void message_handler(char* message){
  //proposal
  if(message[0] == 'p'){
    printf("proposal\n");
  }
  //ack/reply
  else if(message[0] == 'r'){
    printf("ack\n");
  }
  //acceot
  else if(message[0] == 'a'){
    printf("accept\n");
  }
  //nack
  else if(message[0] == 'n'){
    printf("nack\n");
  }
  //poll for update
  else if(message[0] == 'b'){
    printf("poll for update\n");
  }
  //update
  else if(message[0] == 'u'){
    printf("update blockchain\n");
  }
  else{
    printf("unknown message received: %s\n", message);
  }
}

/* UDP SERVER THREAD:
 *  Always receiving UDP messages for Paxos and sending them to other servers without
 *  checking for liveness of receiver.
 * Messages received on UDP: Paxos, accept, prepare, commit?
 */
void* udp_server(void*){
  printf("Initializing UDP Server...\n");
  int sockf;
  char buffer[1024];
  struct sockaddr_in servaddr, cliaddr;

  if ( (sockf = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
    std::cerr << "socket creation failed" << std::endl;
    std::exit(1);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family    = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(ports[id]);

  if(bind(sockf, (const struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 ){
    std::cerr << "bind failed" << std::endl;
    std::exit(1);
  }

  int n;
  socklen_t len;
  while(true){
    memset(&buffer, 0, 1024);
    n = recvfrom(sockf, (char*) buffer, 1024, MSG_WAITALL,
		 (struct sockaddr *) &cliaddr, &len);
    buffer[n] = '\0';
    printf("Message Received: %s", buffer);

    message_handler(buffer);
  }
  pthread_exit(NULL);
}


/* SERVERSETUP
 *  Grab information from the config file, save server/port info
 *  and setup sockets ready for UDP messaging
 *
 *  After setup completes, UDP messages can be sent via:
 *  sendto(sock[i], (const char*) msg, strlen(msg), MSG_CONFIRM)
 */
void serversetup(){
  std::ifstream inFile;
  inFile.open("config.txt");
  if (!inFile) {
    std::cerr << "Unable to open config.txt" << std::endl;
    exit(1);
  }

  printf("Server Info: \n");
  for (int i = 0; i < 5; ++i) {
    std::string portString;
    inFile >> std::skipws >> servers[i];
    inFile >> std::skipws >> portString;
    ports[i] = stoi(portString);
    std::cout << servers[i] << " " << ports[i] << std::endl;

    //Create socket for everyone but yourself
    if(i != id){
      socks[i] = socket(AF_INET, SOCK_DGRAM, 0);
      if (socks[i] < 0 ) {
	std::cerr << "Socket creation failed. Socket ID: " << i << std::endl;
	std::exit(1);
      }

      memset(&servaddrs[i],0,sizeof(servaddrs[i]));

      servaddrs[i].sin_family = AF_INET;
      servaddrs[i].sin_port = htons(ports[i]);
      servaddrs[i].sin_addr.s_addr = inet_addr(servers[i].c_str());
    }
  }
}

int main(int argc, char* argv[]){
  // ./program <id>
  if(argc != 2){
    std::cerr << "usage err" << std::endl;
  }
  id = atoi(argv[1]);

  //Get and store IP/Ports of other servers
  serversetup();

  //Start threads
  int num_threads = 100;
  pthread_t* threads = (pthread_t*) malloc(num_threads);
  memset(threads,0,num_threads);
  int thread_i = 0;

  thread_i++;

  pthread_create(&threads[thread_i], NULL, udp_server, NULL);

  usleep(1000000);
  
  //Start commands
   std::string input;
  printf("Input command: \n");
  std::getline(std::cin, input);

  while(input.compare("exit") != 0){
    //propose <int>
    if((input.at(0) == 'P' || input.at(0) == 'p')){
      //propose(input);
    }
    else{
      printf("Command error.\n");
    }
    printf("Input command: \n");
    std::getline(std::cin, input);
  }
  return 0;

}
