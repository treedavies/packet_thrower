/* SPDX-License-Identifier: GPL-2.0
 *
 * packet_thrower_ctl
 * This program is used to communicate with packet_thrower_server,
 * and tell it what to do.
 *
 * Possible actions:
 * - Listen for packets on a port.
 * - Send packets of size N to an IP and Port.
 * - reset: Stop all operations.
 *
 * Copyright (C) 2017 Tree Davies <tdavies@darkphysics.net>
 * 
 */


#include<stdio.h>   
#include<string.h>  
#include<sys/socket.h>  
#include<arpa/inet.h>   
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#define SIZE 50

int main(int argc , char *argv[])
{
    if (argc != 4) {
	printf("Usage:\n");
	printf("  ./pt_ctl_client <Server> <Port> 'snd,<packet_size>,<Target_IP>,<Target_Port>'\n");
	printf("  ./pt_ctl_client <Server> <Port> 'lsn,<port>'\n");
	printf("  ./pt_ctl_client <Server> <Port> 'reset'\n");
    	return -1;
    }
    
    char* ip   = argv[1];
    int port   = (int)atoi(argv[2]);
    char* cmd  = argv[3];

    int sock;
    struct sockaddr_in server;
    char message[SIZE], server_reply[SIZE];

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        printf("Can not create socket");
    }

    // Fix
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family      = AF_INET;
    server.sin_port        = htons(port);

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
        printf("connection error");
       return 1;
    }

    if(send(sock , cmd, 50 , 0) < 0){
        printf("send error\n");
        return 1;
    }

    //Receive a reply from the server
    memset(server_reply,'\0',SIZE);
    int bytes_recv = 0;
    bytes_recv = recv(sock , server_reply , SIZE, 0);
    printf("\nServer reply :%s\n",server_reply);
        
    fflush(stdout);
    close(sock);
    return 0;
}



