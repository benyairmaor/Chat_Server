//==========================================Includes==========================================//
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include<signal.h> 

#define BUFSIZE 4096

//============================================Node===========================================//
struct Node { //Represnt Node of the Queue - Each Node contain the sender, how to send to and the msg
    int sendTo;
    int  from;
    char* msg;
    struct Node* next; 
} typedef Node;

Node* createNode(int Send, int From, char* Msg){ //Get a fd - the sender, fd - how to send to and the msg. Return Node
    Node* toReturn = (Node*)calloc(1, sizeof(Node));
	if(toReturn == NULL){ //If malloc failed
		return NULL;
	}
    toReturn->sendTo = Send;
    toReturn->from = From;
    toReturn->msg = (char*)calloc(strlen(Msg) + 1, sizeof(char));
	if(toReturn->msg == NULL){ //If malloc failed
		return NULL;
	}
    strcpy(toReturn->msg, Msg);
    toReturn->next = NULL;
    return toReturn;
}

//============================================Queue==========================================//
struct Queue { //Represnt the Queue - The Queue contain int size of the Queue, head and tail
	int size;
	struct Node *head;
	struct Node *tail;
} typedef Queue;
 
Queue* newQueue(){ //Create new Queue
	Queue *q = NULL;
	q = (Queue*)malloc(sizeof(Queue));
	if (q == NULL) {
		return NULL;
	}
	q->size = 0;
	q->head = NULL;
	q->tail = NULL;
	return q;
}
 
int enqueue(Queue *q, Node* node) { //Get a Queue and Node toInsert, insert to the Queue and return the index of this element in the Queue
	if (q->head == NULL) { //If the Queue is empty
		q->head = node;
		q->tail = node;
		q->size = 1;
		return q->size;
	}
	q->tail->next = node; //If the Queue is not empty
	q->tail = node;
	q->size += 1;
	return q->size;
}
 
Node* dequeue(Queue *q){ //Get a Queue and remove the Node from the head if there is any, if not do nothing
	if (q->size == 0) { //If there is noting to remove
		return NULL;
	}
	Node *tmp = NULL;
	tmp = q->head;
	q->head = q->head->next;
	q->size -= 1;
	return tmp;
}
 
void freeQueue(Queue *q){ //Get a Queue and free all of his memory, Nodes and the Queue istself
	if (q == NULL) { //If there is no Queue
		return;
	}
	while (q->head != NULL) {
		Node *tmp = q->head;
		q->head = q->head->next;
		if (tmp->msg != NULL) {
			free(tmp->msg);
		}
		free(tmp);
	}
	free (q);
}

//======================================Global-Variables=====================================//
int mainSocketfd;
Queue* msgQueue;
Queue* resendQueue;
int* fdExist;
int sizeFdExist;

//========================================Array-Method========================================//
int isExist(int target){ //Get a fd and return 1 if the fd in the array of open fds and 0 if he is not
	for(int i = 0; i < sizeFdExist; i++){
		if(fdExist[i] == target){
			return 1;
		}
	}
	return 0;
}

int add(int target){ //Get a fd and add it to the array of the open fds, if there is no place do realloc for make the array biger
	int size = sizeFdExist;
	for(int i = 0; i < size; i++){
		if(fdExist[i] == -1){
			fdExist[i] = target;
			return i;
		}
	}
	sizeFdExist = 2 * sizeFdExist;
	fdExist = (int*)realloc(fdExist, sizeFdExist * sizeof(int)); //If there is no palce make realloc
	if(fdExist == NULL){
		return -1;
	}
	fdExist[size] = target;
	for(int i = size + 1; i < sizeFdExist; i++){
		fdExist[i] = -1;
	}
	return size;
}

int toRemove(int target){ //Get a fd and remove it from the open fds array. if seccses return the index of the remove, if not return -1
	for(int i = 0; i < sizeFdExist; i++){
		if(fdExist[i] == target){
			fdExist[i] = -1;
			return i;
		}
	}
	return -1;
}

//===========================================Handler==========================================//
void handle_sigint(int sig){ //When the server get signale of ctrl+c it get to this handler free all the memory and close the program 
    freeQueue(msgQueue);
	freeQueue(resendQueue);
	for(int i = 1; i < sizeFdExist; i++){
		if(fdExist[i] != -1){
			close(fdExist[i]);
		}
	}
	free(fdExist);
	close(mainSocketfd);
	exit(EXIT_SUCCESS);
} 

//=============================================Main===========================================//
int main(int argc, char *argv[]){ 
//==========================================Validtion=========================================//
    if(argc < 2){ // If the user didn't enter port
        fprintf(stderr, "Usage: server <port>\n");
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    if(port < 0){ //If the port is invalid
        fprintf(stderr, "Usage: server <port>\n");
        exit(EXIT_FAILURE);
    }
	signal(SIGINT, handle_sigint);  //If there is ctrl+c send to handler
//===========================================Conect==========================================//
    int newSocketfd;
    struct sockaddr_in server_addr;
    struct sockaddr client_addr;
    socklen_t client = 0;
    mainSocketfd = socket(AF_INET,SOCK_STREAM,0);
    if(mainSocketfd < 0){
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
//===========================================Bind==========================================//
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    if(bind(mainSocketfd, (struct  sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        close(mainSocketfd);
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
//=========================================Listen==========================================//
    if(listen(mainSocketfd, 10) < 0){
        close(mainSocketfd);
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }
//=====================================Initialization======================================//
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(mainSocketfd, &fds);
    fd_set readfds;
    fd_set writefds;
	int maxSocketNumber = mainSocketfd + 1;
	int rc = 0;
	char buf[4096];
	msgQueue = newQueue();
	if(msgQueue == NULL){ //If newQueue() failed
		close(mainSocketfd);
		perror("malloc failed\n");
        exit(EXIT_FAILURE);
	}
	resendQueue = newQueue();
	if(resendQueue == NULL){ //If newQueue() failed
		close(mainSocketfd);
		freeQueue(msgQueue);
		perror("malloc failed\n");
        exit(EXIT_FAILURE);
	}
	sizeFdExist = 10;
	int flag = 0;
	fdExist = (int*)calloc(sizeFdExist, sizeof(int));
	if(resendQueue == NULL){ //If malloc failed
		close(mainSocketfd);
		freeQueue(msgQueue);
		freeQueue(resendQueue);
		perror("malloc failed\n");
        exit(EXIT_FAILURE);
	}
	for(int i = 0; i < sizeFdExist; i++){
		fdExist[i] = -1;
	}
//===================================Read-And-Write-msgs======================================//
	while(1){
		while(resendQueue->size != 0){ //If threr is msgs that wasn't sent push tham to msgQueue
			Node* temp = dequeue(resendQueue);
			enqueue(msgQueue, temp);
		}
		readfds = fds;
		if(msgQueue->size == 0){ //If there is noting to write set the writer set to ZERO and make the flag 0
			writefds = fds;
			FD_ZERO(&writefds);
			flag = 0;
		}
		else{ //If there is something to write set the writer as fds and make the flag 1
			writefds = fds;
			flag = 1;
		}
		bzero(buf, BUFSIZE); //ZERO the buffer
		rc = select(maxSocketNumber, &readfds, &writefds, NULL, NULL); //Listen to change in any of the sets
		if(rc < 0){ //If select failed
			freeQueue(msgQueue);
			freeQueue(resendQueue);
			for(int i = 1; i < sizeFdExist; i++){
				if(fdExist[i] != -1){
					close(fdExist[i]);
				}
			}
			free(fdExist);
			close(mainSocketfd);
			perror("accept Failed\n");
			exit(EXIT_FAILURE);
		}
		if(FD_ISSET(mainSocketfd, &readfds)){ //If there is something to read from the main socket make accept for new client
			newSocketfd = accept(mainSocketfd, &client_addr, &client);
			if(newSocketfd >= 0){
				FD_SET(newSocketfd, &fds);
				if(newSocketfd >= maxSocketNumber){
					maxSocketNumber = newSocketfd + 1;
				}
				int x = add(newSocketfd);
				if(x == -1){ //If the add failed
					freeQueue(msgQueue);
					freeQueue(resendQueue);
					for(int i = 1; i < sizeFdExist; i++){
						if(fdExist[i] != -1){
							close(fdExist[i]);
						}
					}
					free(fdExist);
					close(mainSocketfd);
					perror("malloc Failed\n");
					exit(EXIT_FAILURE);
				}		
			}
			else{ //If the accept failed
				freeQueue(msgQueue);
				freeQueue(resendQueue);
				for(int i = 1; i < sizeFdExist; i++){
					if(fdExist[i] != -1){
						close(fdExist[i]);
					}
				}
				free(fdExist);
				close(mainSocketfd);
				perror("accept Failed\n");
				exit(EXIT_FAILURE);
			}
		}
		for(int fd = mainSocketfd + 1; fd < maxSocketNumber; fd++){ //Check for all the client if someone write to the socket
			if(FD_ISSET(fd, &readfds)){ //If there is any change in this fd
				rc = read(fd, buf, BUFSIZE);
				if(rc == 0){ //If there is noting to read cilent leave close this socket
					printf("server is ready to read from socket %d\n", fd);
                    close(fd);
					toRemove(fd);
					FD_CLR(fd, &fds);
				}
				else if(rc > 0){ //If there is what to read, read and push it to the msgQueue for all aother clients
					printf("server is ready to read from socket %d\n", fd);
					for(int i = mainSocketfd + 1; i < maxSocketNumber; i++){
						if(i != fd){
							Node *temp = createNode(i, fd, buf);
							enqueue(msgQueue, temp);
						}
					}
				}
				else{ //Read failed
					freeQueue(msgQueue);
					freeQueue(resendQueue);
					for(int i = 1; i < sizeFdExist; i++){
						if(fdExist[i] != -1){
							close(fdExist[i]);
						}
					}
					free(fdExist);
					close(mainSocketfd);
					perror("read Failed\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		if(flag == 1){ //If there is Writer set
			while(msgQueue->size != 0){ //If there is more msg to send
				Node* temp = dequeue(msgQueue);
				if(isExist(temp->sendTo) == 1){ //If the client still conect
					if(FD_ISSET(temp->sendTo, &writefds)){ //If the client ready to write
						char SD[10];
						bzero(SD, 10);
						sprintf(SD, "%d", temp->from);
						printf("Server is ready to write to socket %d\n", temp->sendTo);
						rc = write(temp->sendTo, "guest", strlen("guest"));
						rc = write(temp->sendTo, SD, strlen(SD));
						rc = write(temp->sendTo, ": ", strlen(": "));
						rc = write(temp->sendTo, temp->msg, strlen(temp->msg));
						free(temp->msg);
						free(temp);
					}
					else{ //If the client is not ready to write push to the Resend Queue
						enqueue(resendQueue, temp);
					}
				}
				else{ //The client close the conection throw away this msg
					free(temp->msg);
					free(temp);
				}
			}
		}
	}
    return 0;
}
