#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

/*
Author: Tianyuan Li
server.c for Lab2
*/
#define BUFSIZE 1000

void error();
void getAndSendFile();

int RDT = 0;

int main(int argc, char *argv[])
{
	int sock,check;
	struct sockaddr_in server,client;
	socklen_t length;
	char request[50];
	memset(request,0,sizeof(request));

	//Checks arguments
	if (argc < 2){
		error("Error, please give a port number!\n");
	} else if (argc == 2 || argc == 3) {
		RDT = 0;
	} else if (argc == 4) {
		RDT = 1;
	} else {
		error("Too many arguments\n");
	}

	//Opens socket
	sock = socket(PF_INET,SOCK_DGRAM,0);
	if(sock < 0){
		error("Error opening socket!");
	}

	server.sin_family = PF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(atoi(argv[1]));

	//Binds sockets
	if(bind(sock,(struct sockaddr*)&server,sizeof(server)) < 0){
		error("Error binding socket!\n");
	}

	printf("Waiting for a file...\n");
	length = sizeof(client);
	check = recvfrom(sock,request,sizeof(request),0,(struct sockaddr*)&client,&length);
	getAndSendFile(sock,request,client,length);

	return 0;
}

void error(char *msg){
    perror(msg);
    exit(1);
}

//Finds file and sends file to client
void getAndSendFile(int sock,char filename[],struct sockaddr_in client,socklen_t length){

	char *packet = calloc(1024,sizeof(char));
	char header[3];
	char buffer[BUFSIZE];
	char Ack[2];
	int totRetrans = 0;
	int expected = 0;
	int size,maxSeqNum,sentBytes,packetNum = 0;
	int killSwitch = 0;

	struct stat fsize;

	//Checks if file is available
	if(access(filename,F_OK)!= -1){
		FILE *file =fopen(filename,"rb+");
		stat(filename,&fsize);
		size = fsize.st_size;
		//Finds total number of packets needed for file
		maxSeqNum = size / BUFSIZE;

		sentBytes = 0;
		int packNum = 1;
		clock_t diff, start;

		//Sends the packets
		while(sentBytes < size){
			memset(packet,'\0',1024);
			memset(header,'\0',3);
			memset(buffer,'\0',BUFSIZE);

			header[0] = packetNum % 2;
			header[0] = header[0] + '0';
			if(packetNum < maxSeqNum){
				header[1] = '0';
			}else{
				header[1] = '1';
			}
			strcat(packet,header);
			fread(buffer,1,BUFSIZE,file);
			strcat(packet,buffer);
			sentBytes += sendto(sock,packet,1002,0,(struct sockaddr*)&client,length);
			packetNum++;

			if(RDT == 1) {
				int flag = 1;
				memset(Ack, '\0', 2);
				int x;
				//Starts timer
				start = clock();
				diff = clock() - start;
				int sec = diff / CLOCKS_PER_SEC;
				while (flag) {
					if (packet[1] == '1' && killSwitch >= 10) {
						printf("Receiver termination detected. Terminating Sender.\n");
						exit(1);
					}
					if(sec < 2){
						x = recvfrom(sock, Ack, sizeof(Ack), MSG_DONTWAIT, (struct sockaddr*)&client, &length);
						if (x == 0 || x == -1) {
							diff = clock() - start;
							sec = diff / CLOCKS_PER_SEC;
						} else {
							if (Ack[0] - '0' == expected) {
								printf("Expected ACK %d message received in sender\n", Ack[0]-'0');
								expected = (expected + 1) % 2;
								flag = 0;
								killSwitch = 0;
								//Stops timer;
								packNum++;
							} else {
								printf("Unexpected Ack message %d received in sender\n", Ack[0]-'0');
								totRetrans++;
								printf("Retransmitting packet %d. ", packNum);
								printf("Total number of retransmissions so far: %d\n", totRetrans);
								sendto(sock,packet,1002,0,(struct sockaddr*)&client,length);
								//Restarts timer
								start = clock();
								diff = clock() - start;
								sec = diff / CLOCKS_PER_SEC;
								killSwitch++;
							}
						}
					} else {
						printf("\tTIMEOUT!\n");
						totRetrans++;
						printf("Retransmitting packet %d.  ", packNum);
						printf("Total number of retransmissions so far: %d\n", totRetrans);
						sendto(sock,packet,1002,0,(struct sockaddr*)&client,length);
						//Restarts timer
						start = clock();
						diff = clock() - start;
						sec = diff / CLOCKS_PER_SEC;
						killSwitch++;
					}
				}
			}
		}
		fclose(file);
		close(sock);
	} else {
		printf("\nFile Not Found\n");
	}
}
