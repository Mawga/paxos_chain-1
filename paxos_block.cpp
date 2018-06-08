#include "paxos_block.h"

std::string build_update(){
  std::string update = "Update " + std::to_string(id) + " ";
  std::list<std::vector<Transaction*>>::iterator it1;
  for(it1 = blockchain.begin(); it1 != blockchain.end(); ++it1) {
    //std::vector<Transaction*> *it1
    std::vector<Transaction*>::iterator it2;
    for(it2 = (*it1).begin(); it2 != (*it1).end(); ++it2){
      update += std::to_string((*it2)->amount) + " "
	+ std::to_string((*it2)->from) + " "
	+ std::to_string((*it2)->to);
    }
    update += " chain";
  }
  update += " end";
  return update;
}

void update_balance(){
  int bal = 100;
  std::list<std::vector<Transaction*>>::iterator it1;
  for(it1 = blockchain.begin(); it1 != blockchain.end(); ++it1) {
    std::vector<Transaction*>::iterator it2;
    for(it2 = (*it1).begin(); it2 != (*it1).end(); ++it2){
      if((*it2)->from == id)
        bal -= (*it2)->amount;
      else if ((*it2)->to == id)
        bal += (*it2)->amount;
    }
  }
  balance = bal;
}

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
   * Received: "Prepare <ballot_num> <ballot_id> <depth>"
   * On prepare:
   */
  if(i.compare("Prepare") == 0){
    // Split message up and take values
    std::string ballot_s, id_s, depth_s;
    
    iss >> std::skipws >> ballot_s;
    iss >> std::skipws >> id_s;
    iss >> std::skipws >> depth_s;
    
    int their_ballotnum = stoi(ballot_s);
    int their_id = stoi(id_s);
    int their_depth = stoi(depth_s);
    
    std::cout << "Received from node " << std::to_string(their_id)
	      << ": " << msg_s << std::endl;

    // Check depth for stale prepare
    if (their_depth < depth){
      std::cout << "Stale Prepare Received, sending update" << std::endl;
      // Send update
      // Update amt fr t amt fr t amt fr t chain amt fr t amt fr t amt fr t chain end
      std::string update = build_update();
      sendto(socks[their_id], (const char*) update.c_str(), strlen(update.c_str())
	     , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	     sizeof(servaddrs[their_id]));
    }

    else if (their_depth > depth){
      std::cout << "Depth too low, requesting update" << std::endl;
      std::string request = "Request " + std::to_string(id);
      
      sendto(socks[their_id], (const char*) request.c_str(), strlen(request.c_str())
	     , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	     sizeof(servaddrs[their_id]));
    }
    // ballot check
    else if(their_ballotnum > ballot_num[0]){
      // update ballotnum
      ballot_num[0] = their_ballotnum;
      ballot_num[1] = their_id;
      
      // build reply message:
      // "Reply <ballot_num> <ballot_id> <my_acceptnum> <my_acceptid>
      //        <my_id> <my_acceptval for this ballot_num>"
      std::string b_msg = "Reply " + std::to_string(their_ballotnum) + " "
	+ std::to_string(their_id) + " " + std::to_string(accept_num[0])
	+ " " + std::to_string(accept_num[1]) + " " + std::to_string(id);
      
      // add our acceptval to the message
      std::vector<Transaction>::iterator it;
      for(it = accept_val.begin(); it != accept_val.end(); ++it) {
	b_msg += " " + std::to_string((*it).amount) + " "
	  + std::to_string((*it).from) + " " + std::to_string((*it).to);
      }
      
      b_msg = b_msg +  " " + "end";
      
      // send
      sendto(socks[their_id], (const char*) b_msg.c_str(), strlen(b_msg.c_str())
	     , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	     sizeof(servaddrs[their_id])); 
    }
  }
  
  //"ack"
  //Reply <ballotnum> <ballot_id> <their.acceptnum> <their.acceptid>
  //       <their.id> <their.acceptval> 
  else if(i.compare("Reply") == 0){
    
    // Split message and grab values
    std::string b, i, tan, tai, tid;
    
    iss >> std::skipws >> b;
    iss >> std::skipws >> i;
    iss >> std::skipws >> tan;
    iss >> std::skipws >> tai;
    iss >> std::skipws >> tid;
    
    int ballot = stoi(b);
    int ballot_id = stoi(i);
    int their_acceptnum = stoi(tan);
    int their_acceptid = stoi(tai);
    int their_id = stoi(tid);
    
    std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;
    
    // update relevant global ack counter
    ack[ballot][ballot_id]++;
    
    // ASSUMING ACCEPTVAL IS EMPTY
    // ADD CASE: If acceptval isn't empty 
    
    //majority reached:
    //  broadcast("Accept <ballot_num> <ballot_id> <depth> <my id> <accept_val> ")
    if (ack[ballot][ballot_id] == 3){
      std::cout << "Ack majority reached." << std::endl;
      std::string accept_broad = "Accept " + b + " " + i + " " + std::to_string(depth)
	+ " " + std::to_string(id) + " ";
      
      for(std::vector<Transaction*>::iterator it = queue.begin(); it != queue.end(); ++it) {
	accept_broad += (std::to_string((*it)->amount));
	accept_broad += " ";
	accept_broad += (std::to_string((*it)->from));
   	accept_broad += " ";
   	accept_broad += (std::to_string((*it)->to));
   	accept_broad += " ";
      }
      accept_broad += "end";	
      broadcast((char*)accept_broad.c_str());
      queue.clear();
    }
    // else less than 3 or greater than 3, do nothing
  }
  
  //Accept <acceptnum> <acceptid> <depth> <my id> <acceptval> 
  else if(i.compare("Accept") == 0){
    std::string anum, aid, av, tid, d;
    iss >> std::skipws >> anum;
    iss >> std::skipws >> aid;
    iss >> std::skipws >> d;
    iss >> std::skipws >> tid;
    
    int acceptnum = stoi(anum);
    int acceptid = stoi(aid);
    int blockdepth = stoi(d);
    int their_id = stoi(tid);
    
    int parseMess = 1;

    std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;
    std::cout << "Accepts: " << std::to_string(accepts[acceptnum][acceptid]) << std::endl;
    
    if (accepts[acceptnum][acceptid] < 3){
      std::string amountstr;
      std::string fromstr;
      std::string tostr;
      
      iss >> std::skipws >> amountstr;
      
      if(amountstr.compare("end") == 0 || accept_val.size() != 0){
   	parseMess = 0;
      }
      
      while(parseMess) {
   	iss >> std::skipws >> fromstr;
   	iss >> std::skipws >> tostr;
   	accept_val.push_back(Transaction(stoi(amountstr), stoi(fromstr), stoi(tostr)));
	
	iss >> std::skipws >> amountstr;
	if(amountstr.compare("end") == 0)
	  parseMess = 0;
      }
      
      std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;
      
      // update relevant global accept counter
      accepts[acceptnum][acceptid]++;
      
      // Original proposal will have 2 when he receives first accept, but doesn't matter
      // We want other nodes to broadcast when they're accept reaches 1
      if (accepts[acceptnum][acceptid] == 1){
	std::cout << "First accept received. Broadcast relay." << std::endl;
	// build accept message to be broadcast
	std::string accept_broad = "Accept " + anum + " " + aid +
	  " " + std::to_string(blockdepth) + " " + std::to_string(id) + " ";
	for(std::vector<Transaction>::iterator it = accept_val.begin(); it != accept_val.end(); ++it) {
          accept_broad += std::to_string((*it).amount);
          accept_broad += " ";
          accept_broad += std::to_string((*it).from);
          accept_broad += " ";
          accept_broad += std::to_string((*it).to);
          accept_broad += " ";
	}
        accept_broad += "end";
        broadcast((char*)accept_broad.c_str());
      }
    }
    // Accept reached majority
    else if (accepts[acceptnum][acceptid] == 3){
      std::cout << "Accept reached majority, decide." << std::endl;

      // decide
      std::vector<Transaction*> tempval;
      for(std::vector<Transaction>::iterator it = accept_val.begin(); it != accept_val.end(); ++it) {
	Transaction * temp = new Transaction((*it).amount, (*it).from, (*it).to);
	tempval.push_back(temp);
      }
      accept_val.clear();
      blockchain.push_back(tempval);
      depth++;
      update_balance();
    }
  }
  // Update amt fr t chain amt fr t chain end
  else if(i.compare("Update") == 0){
    std::string amountstr;
    std::string fromstr;
    std::string tostr;
    std::string their_id;
    Transaction* tx;
    blockchain.clear(); // out with the old, in with the new

    iss >> std::skipws >> their_id;
    
    std::cout << "Received from node " << their_id << ": " << msg_s << std::endl;
    
    int parseMess = 1;
    
    iss >> std::skipws >> amountstr;

    std::cout << amountstr << std::endl;
    
    if(amountstr.compare("end") == 0 || amountstr.compare("chain")){
      parseMess = 0;
      std::cout << "end/chain found as first word" << std::endl;
    }
    
    //std::vector<Transaction*>* new_block = new std::vector<Transaction*>();
    while(parseMess) {
      std::cout << "while" << std::endl;
      std::vector<Transaction*> new_block;
      while(amountstr.compare("chain") != 0){
	iss >> std::skipws >> fromstr;
	iss >> std::skipws >> tostr;
	
	tx = new Transaction(stoi(amountstr), stoi(fromstr), stoi(tostr));
      
	new_block.push_back(tx);
      
	iss >> std::skipws >> amountstr;
      }
      
      blockchain.push_back(new_block);

      printblockchain();//
      
      iss >> std::skipws >> amountstr;
      
      if(amountstr.compare("end") == 0)
	parseMess = 0;
    }
    
  }
  else if(i.compare("Request") == 0){
    std::string tid_s;
    iss >> tid_s;
    int their_id = stoi(tid_s);
    std::string update = build_update();

    std::cout << "Received from node " << std::to_string(their_id) << ": " << msg_s << std::endl;
    
    sendto(socks[their_id], (const char*) update.c_str(), strlen(update.c_str())
	   , MSG_CONFIRM, (const struct sockaddr*) &servaddrs[their_id],
	   sizeof(servaddrs[their_id]));
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

    //usleep(1000000);
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
void printblockchain(){
  std::cout << "Blockchain (Amount, From, To): " << std::endl;
  std::list<std::vector<Transaction*>>::iterator it1;
  for(it1 = blockchain.begin(); it1 != blockchain.end(); ++it1) {
    //std::vector<Transaction*> *it1
    std::vector<Transaction*>::iterator it2;
    for(it2 = (*it1).begin(); it2 != (*it1).end(); ++it2){
      std::cout << std::to_string((*it2)->amount) << " "
		<< std::to_string((*it2)->from) << " "
		<< std::to_string((*it2)->to) << std::endl;
    }
  }
}


bool amountexceeded(int amount_s){
  int sendamt = amount_s;
  for(std::vector<Transaction*>::iterator it = queue.begin(); it != queue.end(); ++it) {
    sendamt += (*it)->amount;
  }
  return (sendamt >= balance);
}

void* prop_timeout(void* arg){
  usleep(10000000);

  int sendamt = 0;
  unsigned int i;
  for(i = 0; i < queue.size(); ++i) {
    sendamt += queue[i]->amount;
    if(sendamt > balance){
      break;
    }
  }
  // i is the index at which the send amount exceeded balance
  int remove_index = i;
  for(i; i < queue.size(); ++i) {
    std::cout << "Transaction removed, balance exceeded: "
	      << std::to_string(queue[i]->amount)
	      << "from " << std::to_string(queue[i]->from)
	      << " to " << std::to_string(queue[i]->to)
	      << std::endl;
  }

  queue.resize(remove_index);

  //if queue is now empty, just exit
  if(queue.empty())
    pthread_exit(NULL);
    
  // <ballot_num++, id>
  ballot_num[0]++;
  ballot_num[1] = id;
  
  //initialized ack to 1 because I count towards majority
  ack[ballot_num[0]][id]++;
  accepts[ballot_num[0]][id]++;

  // build message and broadcast
  std::string prep = "Prepare " + std::to_string(ballot_num[0])
    + " " + std::to_string(ballot_num[1]) + " " + std::to_string(depth);
  
  broadcast((char*) prep.c_str());
  pthread_exit(NULL);
}

void printBalance(){
  std::cout << "Balance: " << balance << std::endl;
}

void printQueue(){
  std::cout << "Queue: " << std::endl;
  for(std::vector<Transaction*>::iterator it = queue.begin(); it != queue.end(); ++it) {
    std::cout << "Amount: " << std::to_string((*it)->amount) <<
      " from " << std::to_string((*it)->from) << " to "
	      << std::to_string((*it)->to) << std::endl;
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
  std::string i;
  printf("Input command: \n");
  std::getline(std::cin, input);
  while(input.compare("exit") != 0){
    
    std::istringstream iss(input);
    iss >> std::skipws >> i;
    
    /*
     * On moneyTransfer, broadcast "propose ballot_num++ myId"
     */
    if(i.compare("moneyTransfer") == 0){
      std::string amount_s, debit_s, credit_s;
      iss >> std::skipws >> amount_s;
      iss >> std::skipws >> debit_s;
      iss >> std::skipws >> credit_s;

      if(queue.size() >= 10)
	std::cout << "Queue is full" << std::endl;

      else if(!amountexceeded(stoi(amount_s))){
	// Create new transaction
	Transaction* transaction = new Transaction(0,0,0);
	transaction->amount = stoi(amount_s);
	transaction->from = stoi(debit_s);
	transaction->to = stoi(credit_s);

	// queue of transactions we want to propose
	// accept_val = queue when sending accepts
	queue.push_back(transaction);

	if(queue.size() == 1){
	  thread_i++;
	  pthread_create(&threads[thread_i], NULL, prop_timeout, NULL);
	}
      }
      else{
	std::cout << "Amount exceeds balance" << std::endl;
      }
    }
    else if (i == "printblockchain"){
      printblockchain();
    }
    else if (i == "printqueue")
      printQueue();
    else if(i == "printbalance")
      printBalance();
    else{
      printf("Command error.\n");
    }

    //Short wait for command thread to run properly
    usleep(500000);
    
    printf("Input command: \n");
    std::getline(std::cin, input);
  }
  return 0;

}
