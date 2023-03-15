# 
ex4 – Event-Driven Chat Server
Authored by Sagi Ben Shushan
209351147

==Description for chatServer.c==
The program is implement an event-driven chat server.
The function of the chat is to forward each incoming message’s overall client connections (i.e., to all clients) except for the client connection over which the message was received.
 The challenge in such a server lies in implementing this behavior in an entirely event driven manner (no threads).
i used “select” to check from which socket descriptor is ready for reading or writing/

==Output for chatServer.c==:
Just before calling to select, print: ("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
when accepting a new connection with socket descriptor “sd”, print: ("New incoming connection on sd %d\n", sd);
Before reading data from socket descriptor “sd”, print: ("Descriptor %d is readable\n", sd);
After reading data of size “len” from socket descriptor “sd”, print: ("%d bytes received from sd %d\n", len, sd);
When a connection from a client is closed by the client, print: ("Connection closed for sd %d\n",sd);
When the server removes client’s connection for any reason (either the client closes the connection, or the server closes the client’s connection, for example when closing the server), print: ("removing connection with sd %d \n", sd);
In case the server is closed, you should print the above line for each active client (not for the main socket).

==Error Handling of chatServer.c==
If there is no port parameter, or more than one parameter, or the parameter is not a number between 1 to 2^16, print: ("Usage: server <port>”);
For each failure, until ‘select’ is called, I use perror and exit the server. 
If select fails, exit the loop, clean the memory, and exit.
For any other error, skip the operation and continue. I.e., if read from a socket or write to socket fail, do nothing, just continue, and skip this read or write operation.

==Functions of chatServer.c==
void intHandler(int SIG_INT); 
int main (int argc, char *argv[]);
int init_pool(conn_pool_t* pool);
int add_conn(int sd, conn_pool_t* pool);
int remove_conn(int sd, conn_pool_t* pool);
int add_msg(int sd,char* buffer,int len,conn_pool_t* pool);
int write_to_client(int sd,conn_pool_t* pool);

--The description for each function is at the beginning of the function-- 

==Program Files==
chatServer.c










