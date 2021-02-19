# Chat_Server

===Description ===

Program files:

chatserver.c - server that handle multiplay user. Send msg to all of other user that connect to the server, and receive text msg from any other user that conect to the server. Each msg will be no more than 4096. Each msg sending by press Enter at the end of the msg. 
	
funcation:

	struct Node - Represnt Node of the Queue - Each Node contain the sender, how to send to and the msg.
	Node* createNode(int Send, int From, char* Msg) - Get a fd - the sender, fd - how to send to and the msg. Return Node.
	struct Queue - Represnt the Queue - The Queue contain int size of the Queue, head and tail.
	Queue* newQueue() - Create new Queue.
	int enqueue(Queue *q, Node* node) - Get a Queue and Node toInsert, insert to the Queue and return the index of this element in the Queue.
	Node* dequeue(Queue *q) - Get a Queue and remove the Node from the head if there is any, if not do nothing.
	void freeQueue(Queue *q) - Get a Queue and free all of his memory, Nodes and the Queue istself.
	int isExist(int target) - Get a fd and return 1 if the fd in the array of open fds and 0 if he is not.
	int add(int target) - Get a fd and add it to the array of the open fds, if there is no place do realloc for make the array biger.
	int toRemove(int target) - Get a fd and remove it from the open fds array. if seccses return the index of the remove, if not return -1.
	void handle_sigint(int sig) - When the server get signale of ctrl+c it get to this handler free all the memory and close the program.
	int main(int argc, char *argv[]) - the main program wait with select until there is any change and read and write the msg between the users
