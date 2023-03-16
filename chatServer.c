//Sagi Ben Shushan
//209351147
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "chatServer.h"
#include "string.h"

static int end_server = 0;

void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    end_server = 1;
}

int main (int argc, char *argv[])
{
    int port = -1;
    int index_of_port = 1;
    if(argc != 2){
        printf( "Usage: server <port>\n");
        exit(1);
    }
    port = atoi(argv[index_of_port]);
    if(port < 1 || port > 65535){
        printf("Usage: server <port>\n");
        exit(1);
    }
    signal(SIGINT, intHandler);

    conn_pool_t* pool = malloc(sizeof(conn_pool_t));     //allocation
    if(pool == NULL){
        perror("Allocation of the pool failed!\n");
        exit(1);
    }
    init_pool(pool);

    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/
    int wellcome_socketFd = -1;
    if ((wellcome_socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket\n");
        free(pool);
        exit(1);
    }
    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    int on = 1;
    int rc = -1;
    rc = ioctl(wellcome_socketFd,(int)FIONBIO,(char*)&on);               //ioctl
    if(rc < 0){
        perror("ioctl\n");
        free(pool);
        close(wellcome_socketFd);
        exit(1);
    }
    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    if((bind(wellcome_socketFd,(struct sockaddr*) &srv,sizeof(srv))) < 0){     //bind
        perror("bind\n");
        free(pool);
        close(wellcome_socketFd);
        exit(1);
    }
    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    if(listen(wellcome_socketFd,5) < 0){                           //listen
        perror("listen\n");
        free(pool);
        close(wellcome_socketFd);
        exit(1);
    }
    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_SET(wellcome_socketFd, &pool->read_set);
    int maximum = 0;
    struct conn* H;
    struct conn* N;
    pool->maxfd = wellcome_socketFd;
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do
    {
        if(pool->maxfd == -1){
            pool->maxfd = wellcome_socketFd;
        }
        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/

        memcpy(&pool->ready_read_set, &pool->read_set, (int)(pool->nr_conns + 1));
        memcpy(&pool->ready_write_set, &pool->write_set, (int)(pool->nr_conns + 1));
        /**********************************************************/
        /* Call select()*/
        /**********************************************************/
        //Just before calling to select, print:
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        maximum = (pool->maxfd + 1);
        pool->nready = select(maximum,&pool->ready_read_set,&pool->ready_write_set,NULL,NULL);
        //If select fails, exit the loop, clean the memory, and exit.
        if(pool->nready < 0) {
            break;
        }
        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/
        int counter = 0;
        int length_of_msg = 0;
        int new_fd = -1;                         //returned by accept()
        char buf[BUFFER_SIZE] = "";
        memset(buf, '\0', sizeof(char)*(BUFFER_SIZE));
        for (int index = 0; index <= pool->maxfd ; index++)
        {
            if(counter == pool->nready){
                break;
            }
            if (FD_ISSET(index, &pool->ready_read_set))
            {
                counter++;
                if(index == wellcome_socketFd)
                {
                    new_fd = accept(wellcome_socketFd, NULL, NULL);  //accept
                    if (new_fd < 0){
                        // For any other error, skip the operation and continue.
                        continue;
                    }
                    add_conn(new_fd, pool);
                    printf("New incoming connection on sd %d\n", new_fd);
                    continue;
                }
                //before reading data from socket descriptor “sd”, print:
                printf("Descriptor %d is readable\n", index);
                length_of_msg = (int)read(index, buf, (BUFFER_SIZE - 1) * sizeof(char));
                if (length_of_msg == 0){
                    printf("Connection closed for sd %d\n",index);
                    remove_conn(index, pool);
                    memset(buf, '\0', sizeof(char)*(BUFFER_SIZE));
                }
                else{
                    printf("%d bytes received from sd %d\n", length_of_msg, index);
                    add_msg(index, buf, length_of_msg, pool);
                    memset(buf, '\0', sizeof(char)*(BUFFER_SIZE));
                }
            }
            if (FD_ISSET(index, &pool->ready_write_set)) {
                counter++;
                write_to_client(index, pool);
            }

        } /* End of loop through selectable descriptors */
    } while (end_server == 0);
    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/
    H = NULL;
    N = NULL;
    H = pool->conn_head;
    while(H != NULL){
        N = H->next;
        remove_conn(H->fd, pool);
        H = N;
    }
    N = NULL;
    printf("removing connection with sd %d \n", wellcome_socketFd);
    close(wellcome_socketFd);
    free(pool);
    return 0;
}

int init_pool(conn_pool_t* pool) {
    //initialized all fields
    /* Largest file descriptor in this pool. */
    pool->maxfd = -1;
    /* Number of ready descriptors returned by select. */
    pool->nready = 0;
    /* Set of all active descriptors for reading. */
    FD_ZERO(&pool->read_set);
    /* Subset of descriptors ready for reading. */
    FD_ZERO(&pool->ready_read_set);
    /* Set of all active descriptors for writing. */
    FD_ZERO(&pool->write_set);
    /* Subset of descriptors ready for writing.  */
    FD_ZERO(&pool->ready_write_set);
    /* Doubly-linked list of active client connection objects. */
    pool->conn_head = NULL;
    /* Number of active client connections. */
    pool->nr_conns = 0;
    return 0;
}

int add_conn(int sd, conn_pool_t* pool) {
    /*
     * 1. allocate connection and init fields
     * 2. add connection to pool
     * */
    conn_t *new_connection;
    new_connection = (conn_t*)malloc(sizeof(conn_t));
    //init
    new_connection->write_msg_head = NULL;
    new_connection->write_msg_tail = NULL;
    new_connection->prev = NULL;
    new_connection->next = NULL;
    new_connection->fd = sd;

    if(pool->nr_conns != 0){
        new_connection->next = pool->conn_head;
        pool->conn_head->prev = new_connection;
        pool->conn_head = new_connection;
    }
    else{
        pool->conn_head = new_connection;   //add the first connection
        pool->conn_head->prev = NULL;
        pool->conn_head->next = NULL;
    }
    if(pool->maxfd < sd) {
        pool->maxfd = sd;
    }
    FD_SET(sd, &pool->read_set);
    (pool->nr_conns)++;           //update num of connections
    return 0;
}

int remove_conn(int sd, conn_pool_t* pool) {
    /*
	* 1. remove connection from pool
	* 2. deallocate connection
	* 3. remove from sets
	* 4. update max_fd if needed
	*/
    printf("removing connection with sd %d \n", sd);
    conn_t* p = pool->conn_head;
    conn_t *check_max = pool->conn_head;
    int current_max = pool->maxfd;
    int new_max = 0;
    while(p != NULL){
        if(p->fd == sd){
            break;
        }
        p = p->next;
    }
    if (current_max == p->fd){     //updating max_fd
        while (check_max != NULL){
            if(check_max->fd > new_max && check_max->fd != current_max) {
                new_max = check_max->fd;
            }
            check_max = check_max->next;
        }
        pool->maxfd = new_max;
    }
    if(pool->nr_conns == 1){
        pool->conn_head = NULL;
    }
    else if(p->next == NULL){         //The last connection in the list
        p->prev->next = NULL;
    }
    else if(p->prev == NULL){         //The connection is the head of the list
        pool->conn_head = p->next;
        p->next->prev = NULL;
    }
    else{                             //The connection is in the middle of the list
        p->prev->next = p->next;
        p->next->prev = p->prev;
    }
    close(p->fd);
    (pool->nr_conns)--;
    FD_CLR(p->fd, &pool->read_set);
    msg_t *m_pointer = p->write_msg_head;
    msg_t *temp = NULL;
    while (m_pointer != NULL){  //make Frees to ant msg
        temp = m_pointer->next;
        free(m_pointer->message);
        free(m_pointer);
        m_pointer = temp;
    }
    temp = NULL;
    FD_CLR(p->fd, &pool->write_set);
    free(p);
    return 0;
}

int add_msg(int sd,char* buffer,int len,conn_pool_t* pool) {
    /*
	 * 1. add msg_t to write queue of all other connections
	 * 2. set each fd to check if ready to write
	 */
    msg_t *m = NULL;
    conn_t *pointer = pool->conn_head;
    for (; pointer != NULL; pointer = pointer->next){
        if (pointer->fd != sd){
            m = (msg_t*)malloc(sizeof(msg_t));
            //init message
            m->size = len;
            m->next = NULL;
            m->prev = NULL;
            m->message = (char*)malloc(sizeof(char)*(len + 1));
            memset(m->message,'\0',(len + 1));
            strcpy(m->message,buffer);
            if (pointer->write_msg_head == NULL){
                pointer->write_msg_head = m;
                pointer->write_msg_tail = m;
            } else {
                pointer->write_msg_tail->next = m;
                m->prev = pointer->write_msg_tail;
                pointer->write_msg_tail = m;
            }
            FD_SET(pointer->fd, &pool->write_set);
        }
        else{
            continue;
        }
    }
    return 0;
}

int write_to_client(int sd,conn_pool_t* pool) {
    /*
	 * 1. write all msgs in queue
	 * 2. deallocate each writen msg
	 * 3. if all msgs were writen successfully, there is nothing else to write to this fd... */
    msg_t *m = NULL;
    conn_t *ptr_to_sd = pool->conn_head;
    while(ptr_to_sd != NULL){
        if(ptr_to_sd->fd == sd){
            m = ptr_to_sd->write_msg_head;
            break;
        }
        ptr_to_sd = ptr_to_sd->next;
    }
    msg_t* temp = NULL;
    while(m != NULL){
        temp = m->next;
        write(ptr_to_sd->fd, m->message, m->size);
        free(m->message);
        free(m);
        m = temp;
    }
    ptr_to_sd->write_msg_tail = NULL;
    ptr_to_sd->write_msg_head = NULL;
    FD_CLR(ptr_to_sd->fd, &pool->write_set);
    return 0;
}

