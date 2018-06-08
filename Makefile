all:
	gcc -std=c++11 -Wall -g -o paxos_block paxos_block.cpp -lpthread
clean:
	find . -name "*~" -type f -delete
	rm paxos_block
