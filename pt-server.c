/* SPDX-License-Identifier: GPL-2.0
 *
 * packet_thrower server
 * Listens on a port, and accepts command from pt_ctl_client
 *
 * Copyright (C) 2017 Tree Davies <tdavies@darkphysics.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#define RESERVED_PORTS 1024
#define MAX_PAYLOAD    65507
#define PORT_LIMIT     65535
#define MAX_MSG_SIZE   1000

int num_procs = 0;
int procs_running[100] = {0};

static const int MAXPENDING = 5; // Maximum outstanding connection requests

void SIGQUIT_handler(){
    exit(0);
}


/*-----------------------------------
Throw packets on the wire.
-----------------------------------*/
void do_udp_flow(int pkt_sz, char* servIP, int port) {

    int sent; 
    int sock;
    struct sockaddr_in destAddr;

    char* payload;
    payload = malloc(sizeof(sizeof(char) * pkt_sz));
    memset(payload, 'X',pkt_sz);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Error:");
        exit(1);
    }

    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family      = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(servIP);
    destAddr.sin_port        = htons(port);

    while (1) {
        sent = sendto(sock, payload, pkt_sz, 0, (struct sockaddr *) &destAddr, 
               sizeof(destAddr));
        if ( sent != pkt_sz ) {
            perror("send Error:");
            exit(1);
        }
    }

    //close(sock);
    //exit(0);
}



/*--------------------------------
-----------------------------------*/
void do_udp_lsn(int port, int sock, struct sockaddr_in ServAddr) {

    int packets_recvd = 0;
    struct sockaddr_in ClntAddr;
    unsigned int cliAddrLen;
    char Buffer[MAX_PAYLOAD];
    int recvMsgSize;

    char cp_buffer[7000];
    memset(cp_buffer,'\0',6999);
    for (;;) {
        cliAddrLen = sizeof(ClntAddr);

        if ( (recvMsgSize = recvfrom(sock, Buffer, MAX_PAYLOAD, 0, 
             (struct sockaddr *) &ClntAddr, &cliAddrLen) ) < 0) { 
               perror("Recv Error: ");
        }
        memcpy(cp_buffer, Buffer, MAX_PAYLOAD);
    }
}


/*------------------------------------
------------------------------------*/
int interp_snd_cmd(char tmp[]) {

    char* match;         
    int pkt_sz, dest_port;

    /* Process snd messages. Example: "snd,65507,192.168.1.42,2727" */
    // TODO Feature: Add a start time.
    match = strstr(tmp, "snd");
    if(match) {
        char* token = strtok(tmp, ","); // rtn the word snd
        char* psize = strtok(NULL,",");
        char* dest  = strtok(NULL, ",");
        char* port  = strtok(NULL, ",");
        if (!(psize && dest && port )) {
            printf("interp_snd_cmd parse error");
            return -1;
        }

        pkt_sz    = (int)atoi(psize);
        dest_port = (int)atoi(port);

        if( !(pkt_sz > 0 && pkt_sz <= MAX_PAYLOAD &&
             dest_port > RESERVED_PORTS && dest_port <= PORT_LIMIT) ) {
            printf("Error: Incorrect Parameter Sizes/Limits");
            return -1;
        }

        /*TODO Add validate IP address check here  */

        int pid = fork();
        if(pid < 0 ) {
            printf("\nFork Error: interp_snd_cmd() \n"); 
            return -1;
        }

        if(pid == 0) { // Child proc
            if (signal(SIGQUIT, SIGQUIT_handler) == SIG_ERR) {
                printf("\nSIGQUIT install error\n");
                return -1;
            }
            do_udp_flow(pkt_sz, dest, dest_port);
        }
        else{ // Parent
           return pid;
        }

    }

    return -1;
}


/*--------------------------------
-----------------------------------*/
int interp_lsn_cmd(char tmp[]) {

    struct sockaddr_in ServAddr;
    char* token;
    int pkt_sz;
    int dest_port;
    unsigned short lsn_port;
    int sock;

    //assert token == "lsn"
    token = strtok(tmp,",");
    char* port  = strtok(NULL,",");
    if (!port) {
        return -1;
    }

    lsn_port = atoi(port);
    if( !(lsn_port > RESERVED_PORTS && lsn_port < PORT_LIMIT) ) {
        printf("\nPort Range Error");
        return -1;
    }

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("\nSocket Acquire Failure");
        return -1;
    }

    int optval = 1;
    int optlen = sizeof(optval);
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optlen)) < 0) {
        printf("setsockopt failed\n");
        close(sock);
        return -1;
    }


    memset(&ServAddr, 0, sizeof(ServAddr));
    ServAddr.sin_family      = AF_INET;
    ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ServAddr.sin_port        = (unsigned short) htons(lsn_port);

    if (bind(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0){
        perror("Bind Error");
        return -1; 
    }

    int pid = fork();
    if(pid < 0) {
        printf("\nError Fork: interp_lsn_cmd() \n");
        return -1;
    }

    if(pid == 0) { // Child proc
        if (signal(SIGQUIT, SIGQUIT_handler) == SIG_ERR) {
            printf("\nSIGQUIT install error\n");
            exit(2);
        }
        do_udp_lsn(dest_port, sock, ServAddr);
    }
    else{ // Parent
       printf("return pid of child");
       return pid;
    }

    return -1;
}


/*--------------------------------
-----------------------------------*/
int interpret(char tmp[]) {

    printf("\nInterpret:%s",tmp);

    char* match;
    match = strstr(tmp, "snd");
    if(match) {
        return interp_snd_cmd(tmp);
    }

    match = strstr(tmp, "lsn");
    if(match) {
        return interp_lsn_cmd(tmp);
    }
    
    match = strstr(tmp,"reset");
    if(match) {
        int i;
        for(i = 0; i < num_procs; i++) {
            printf("\nKilling %d",procs_running[i]);

            int ks = kill(procs_running[i],SIGTERM);
            if (ks != 0){
                printf("\nSIGTERM PID:%d FAILED",procs_running[i]);
                return -1;
            }

            int* rtn;
            waitpid(procs_running[i], rtn, 0);
        }       
        num_procs = 0;
        return 0;
    }

    return -1;
}


/*----------------------------------
-----------------------------------*/
int main(int argc, char *argv[]) {

    int  servSock;
    char client_message[MAX_MSG_SIZE];
    memset(client_message, '\0',MAX_MSG_SIZE);

    if (argc != 2) { 
	printf("Usage:\n");
        printf("  pt_server <listen-port> \n");
        return -1;
    }
    in_port_t servPort = atoi(argv[1]); 
    // verify
    
    servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (servSock < 0) {
        printf("\n%d",servSock);
        perror("Socket create Error");
        fflush(stdout);
    }

     
    struct sockaddr_in servAddr;                  
    memset(&servAddr, 0, sizeof(servAddr));       
    servAddr.sin_family = AF_INET;                
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servAddr.sin_port = (unsigned short) htons(servPort);          
    
    if (bind(servSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
        printf("Socket create error");
    }
    if (listen(servSock, 5) < 0) {
        perror("Listen error");
    }

    struct sockaddr_in clntAddr; // Client address
    // Set length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    for (;;) { 
        int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
        if (clntSock < 0) {
            perror("Socket Accept Error");
        }
        printf("Client Accepted");

        memset(client_message, '\0',1000);
        int read_size = recv(clntSock, client_message, sizeof(client_message), 0);
        printf("\nmessage recieved: %d Bytes, %s",read_size,client_message);

        int rtn = interpret(client_message);
        printf("\nInterpret returned: %d",rtn);
        if(rtn < 0) {
            printf("\nError Interpreting msg:");
            char fail[] = "-1";
            int write_size = write(clntSock , fail , strlen(fail));
        }
        else {
            printf("\nSuccessful Exec command.\n\n");
            if(rtn > 0) { 
                procs_running[num_procs] = rtn; //PID of child
                num_procs++;
            }

            char pass[] = "0";
            int write_size = write(clntSock , pass , strlen(pass));
            fflush(stdout);
            close(clntSock);

        }
        fflush(stdout); 
    }

}



