# paxos_chain
CS171 Final Project
Maga Kim and Mario Infante

Development Plan:
1. One server takes input "Propose 5", code a function to send proposal to all via UDP
2. Code a message handler for when a proposal is received. Send ack or ignore/nack?
3. Code <something> that is counting the acks it receives after a proposal is sent out. If not enough reached before timeout, query for update or resend with new ballot ID.
3B. When majority of acks received, send "accept 5" to all.
3C. When first "accept 5" is received, send to all
3D. When majority of accepts received, add value to replicated log.


Current Questions:
Do we implement leader election so that only leader handles sending accepts out?
How will we implement an ack timeout?
How will a dead server come back and query for an update?