#include "paxos_block.h"

/* MESSAGE HANDLER THREAD:
 * Handle any message received from UDP/TCP servers and respond accordingly
 */
void* message_handler(void* arg){
  char* message = (char*) arg;
  std::string msg_s(message);
  std::istringstream iss(msg_s);

  /*
   * Received: "Prepare <ballot> <their.id>"
   * On prepare:
   *   
   */
  if(message[0] == 'P'){
    std::string p,b,i;
    iss >> std::skipws >> p;
    iss >> std::skipws >> b;
    iss >> std::skipws >> i;
    int their_ballotnum = stoi(b);
    int their_id = stoi(i);
    std::cout << "Received from node " << std::to_string(their_id)
	      << ": " << msg_s << std::endl;

    // ballot check
    if(their_ballotnum >= ballot_num[0]){

      if(their_ballotnum == ballot_num[0] && their_id < ballot_num[1]){
	//do nothing, I have higher ballotnum
      }

      // they have higher ballotnum
      else{
	// update ballotnum
	ballot_num[0] = their_ballotnum;	

	// build reply message
	std::string b_msg = "Reply " + std::to_string(their_ballotnum) + " "
	  + std::to_string(their_id) + " " + std::to_string(accept_num[0])
	  + " " + std::to_string(accept_num[1]) + " " + std::to_string(accept_val)
	  + " " + std::to_string(id);

	// send
	sendto(socks[their_id], (const char*) b_msg.c_str(), strlen(b_msg.c_str())
	       , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	       sizeof(servaddrs[their_id]));
      }
    }
  }

  //"ack"
  //Reply <ballotnum> <id> <their.acceptnum> <their.acceptid> <their.acceptval> <their.id>
  else if(message[0] == 'R'){
    // update relevant global ack counter
    std::string r, b, i, tan, tai,tav, tid;
    iss >> std::skipws >> r;
    iss >> std::skipws >> b;
    iss >> std::skipws >> i;
    iss >> std::skipws >> tan;
    iss >> std::skipws >> tai;
    iss >> std::skipws >> tav;
    iss >> std::skipws >> tid;
    int ballot = stoi(b);
    int ballot_id = stoi(i);
    int their_acceptnum = stoi(tan);
    int their_acceptid = stoi(tai);
    int their_acceptval = stoi(tav);
    int their_id = stoi(tid);
    std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;
  }
  
  //Accept <ballotnum> <id> <acceptval> <their.id>
  else if(message[0] == 'A' || message[0]){
    // update relevant global accept counter
  }
  //nack
  else if(message[0] == 'N'){
    printf("nack\n");

    // usage tbd...
  }
  //poll for update
  else if(message[0] == 'B'){
    printf("poll for update\n");

    // send replicated log
  }
  //update
  else if(message[0] == 'U'){
    printf("update blockchain\n");

    // update local replicated log
  }
  else{
    printf("unknown message received: %s\n", message);
  }
  pthread_exit(NULL);
}

/* UDP SERVER THREAD:
 *  Always receiving UDP messages for Paxos and sending them to other servers without
 *  checking for liveness of receiver.
 * Messages received on UDP: Paxos, accept, prepare, commit?
 */
void* udp_server(void*){
  printf("Initializing UDP Server...\n");
  int sockfd;
  char buffer[1024];
  struct sockaddr_in servaddr, cliaddr;

  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
    std::cerr << "socket creation failed" << std::endl;
    std::exit(1);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family    = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(ports[id]);

  if(bind(sockfd, (const struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 ){
    std::cerr << "bind failed" << std::endl;
    std::exit(1);
  }

  int n;
  socklen_t len;
  while(true){
    memset(&buffer, 0, 1024);
    n = recvfrom(sockfd, (char*) buffer, 1024, MSG_WAITALL,
		 (struct sockaddr *) &cliaddr, &len);
    if(n < 0){
      std::cout << "recvfrom returned: " << n << std::endl;
      std::exit(1);
    }
    buffer[n] = '\0';
    
    // add message to heap before passing over to thread
    char* arg = (char*) malloc(strlen(buffer));
    strcpy(arg, buffer);
    thread_i++;
    pthread_create(&threads[thread_i], NULL, message_handler, (void*) arg);

    usleep(1000000);
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

/*
 * Broadcast an input to ALL servers
 */
void broadcast(char* input){
  printf("Broadcast: %s\n", input);
  for(int i = 0; i < 5; i++){
    sendto(socks[i], (const char*) input, strlen(input), MSG_CONFIRM,
	   (const struct sockaddr*) &servaddrs[i], sizeof(servaddrs[i]));
  }
}

int main(int argc, char* argv[]){
  // ./program <id>
  if(argc != 2){
    std::cerr << "Usage: ./paxos_block <id>" << std::endl;
  }

  // update id accordingly
  id = atoi(argv[1]);

  // Get and store network variables
  serversetup();

  // Start threads
  threads = (pthread_t*) malloc(num_threads);
  memset(threads,0,num_threads);
  thread_i++;

  // UDP Server threads, constantly listening and handling incoming messages
  pthread_create(&threads[thread_i], NULL, udp_server, NULL);

  // Wait 1 second to let udp_server to start up
  usleep(1000000);
  
  // Start allowing for command input 
  std::string input;
  printf("Input command: \n");
  std::getline(std::cin, input);
  while(input.compare("exit") != 0){
    /*
     * On propose, broadcast "propose ballot_num++ myId"
     */
    if((input.at(0) == 'P' || input.at(0) == 'p')){
      ballot_num[0]++;
      ballot_num[1] = id;
      std::string prep = "Prepare " + std::to_string(ballot_num[0])
	+ " " + std::to_string(ballot_num[1]);
      broadcast((char*)prep.c_str());
    }
    else{
      printf("Command error.\n");
    }

    //Short wait for command thread to run properly
    usleep(1000000);
    
    printf("Input command: \n");
    std::getline(std::cin, input);
  }
  return 0;

}


/* NOT POSSIBLE? atttempts: boost::timeout, std::future
 *
 * Constantly check if a majority of acks or accepts has been achieved and return true.
 * At timeout, the function will be terminated regardless of majority achieved and assume
 * false.
 *
bool majority_counter(char* arg){
  std::string arg_s(arg);
  std::istringstream iss(arg_s);
  
  // Counting Acks
  if(arg[0] == 'R'){
    std::string x_s, y_s;
    iss >> std::skipws >> x_s;
    iss >> std::skipws >> y_s;
    int x = stoi(x_s);
    int y = stoi(y_s);
    while(ack[x][y] < 3){
      usleep(1000000);
    }
  }

  // Counting Accepts
  else{
    std::string x_s, y_s;
    iss >> std::skipws >> x_s;
    iss >> std::skipws >> y_s;
    int x = stoi(x_s);
    int y = stoi(y_s);
    while(accepts[x][y] < 3){
      usleep(1000000)
    }
  }
  return true;
}
*/
