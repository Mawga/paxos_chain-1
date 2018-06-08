#include "paxos_block.h"

/* MESSAGE HANDLER THREAD:
 * Handle any message received from UDP/TCP servers and respond accordingly
 */
void* message_handler(void* arg){
  char* message = (char*) arg;
  std::string msg_s(message);
  std::string i;
  std::istringstream iss(msg_s);

  // grab first word
  iss >> std::skipws >> i;

  /*
   * Received: "Prepare <ballot_num> <ballot_id>"
   * On prepare:
   */
  if(i.compare("Prepare") == 0){
    // Split message up and take values
    std::string ballot_s, id_s;
    iss >> std::skipws >> ballot_s;
    iss >> std::skipws >> id_s;
    int their_ballotnum = stoi(ballot_s);
    int their_id = stoi(id_s);

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

	// build reply message:
	// "Reply <ballot_num> <ballot_id> <my_acceptnum> <my_acceptid>
	//        <my_acceptval for this ballot_num> <my_id>"
	std::string b_msg = "Reply " + std::to_string(their_ballotnum) + " "
	  + std::to_string(their_id) + " " + std::to_string(accept_num[0])
	  + " " + std::to_string(accept_num[1]) + " "
	  + std::to_string(accept_val[their_ballotnum]) + " " + std::to_string(id);

	// send
	sendto(socks[their_id], (const char*) b_msg.c_str(), strlen(b_msg.c_str())
	       , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	       sizeof(servaddrs[their_id]));
      }
    }
  }

  //"ack"
  //Reply <ballotnum> <ballot_id> <their.acceptnum> <their.acceptid>
  //       <their.acceptval> <their.id>
  else if(i.compare("Reply") == 0){

    // Split message and grab values
    std::string b, i, tan, tai,tav, tid;
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
    
    // update relevant global ack counter
    ack[ballot][ballot_id]++;

    //majority reached:
    //  broadcast("Accept <ballot_num> <ballot_id> <accept_val> <my_id>")
    if (ack[ballot][ballot_id] == 3){
      std::string accept_broad = "Accept " + b + " " + i + " "  +
	+ " " + std::to_string(accept_val[ballot]) + " " + std::to_string(id);
      broadcast((char*)accept_broad.c_str());
    }
    // else less than 3 or greater than 3, do nothing
  }
  
  //Accept <acceptnum> <acceptid> <acceptval> <their.id>
  else if(i.compare("Accept") == 0){
    std::string anum, aid, av, tid;
    iss >> std::skipws >> anum;
    iss >> std::skipws >> aid;
    iss >> std::skipws >> av;
    iss >> std::skipws >> tid;
    int acceptnum = stoi(anum);
    int acceptid = stoi(aid);
    int acceptval = stoi(av);
    int their_id = stoi(tid);

    std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;

    // update relevant global accept counter
    accepts[acceptnum][acceptid]++;

    // Original proposal will have 2 when he receives first accept, but doesn't matter
    // We want other nodes to broadcast when they're accept reaches 1
    if (accepts[acceptnum][acceptid] == 1){
      // build accept message to be broadcast
      std::string accept_broad = "Accept " + anum + " " + aid +
	" "  + av + " " + std::to_string(id);
      broadcast((char*)accept_broad.c_str());
    }
    // Accept reached majority
    else if (accepts[acceptnum][acceptid] == 3){
      // decide
      log.push_back(acceptval);
    }
  }
  /* NOT IN USE *********************
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
  * NOT IN USE **************************
  */  
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
 *     sendto(socks[i], (const char*) input, strlen(input), MSG_CONFIRM,
 *	   (const struct sockaddr*) &servaddrs[i], sizeof(servaddrs[i]));
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

/*
 * Print the log neatly
 */
void printlog(){
  std::cout << "Log: ";
  for(std::vector<int>::iterator it = log.begin(); it != log.end(); ++it) {
    std::cout << *it << " ";
  }
  std::cout << std::endl;
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
  std::string i;
  printf("Input command: \n");
  std::getline(std::cin, input);
  while(input.compare("exit") != 0){
    
    std::istringstream iss(input);
    iss >> std::skipws >> i;
    
    /*
     * On propose, broadcast "propose ballot_num++ myId"
     */
    if((i == "Propose") || (i == "propose") || (i == "prepare") || (i == "Prepare")){
      std::string v;
      iss >> std::skipws >> v;

      // <ballot_num++, id>
      ballot_num[0]++;
      ballot_num[1] = id;

      //initialized ack to 1 because I count towards majority
      ack[ballot_num[0]][id]++;
      accepts[ballot_num[0]][id]++;
       
      // acceptval of this ballotnum = v
      accept_val[ballot_num[0]] = stoi(v);

      // build message and broadcast
      std::string prep = "Prepare " + std::to_string(ballot_num[0])
	+ " " + std::to_string(ballot_num[1]);

      broadcast((char*) prep.c_str());
    }
    else if (i == "printlog"){
      printlog();
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
