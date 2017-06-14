#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>


/*
Author: Tianyuan Li
client.c for Lab2
*/
#define BUFSIZE 1024

void error();
void RDTSend();
void RegularSend();

float loss;

int main(int argc, char  *argv[]){
	int sock, portNum, connection;
	int packCount = 0;
	int finish = 0;
	int currPack = 0;
	float random;
	int expected = 0;
	int RDT = 0;
	struct sockaddr_in server_add;
	char request[50];

	if(argc < 4){
		error("Not enough arguements! At least needs to be IP address, port number, filename");
	} else if (argc == 4) {
		//Does nothing
	} else if (argc == 5) {
		loss = atof(argv[4]);
		RDT = 0;
	} else if (argc == 6) {
		loss = atof(argv[4]);
		RDT = 1;
	} else {
		error("Too many arguments!\n");
	}

	portNum = atoi(argv[2]);


	sock = socket(PF_INET,SOCK_DGRAM, 0);
	if(sock < 0){
		error("Error opening the socket!");
	}

	server_add.sin_family = AF_INET;
	inet_pton(PF_INET, argv[1], &server_add.sin_addr);
	server_add.sin_port = htons(portNum);

	connection = connect(sock,(struct sockaddr*)&server_add,sizeof(struct sockaddr_in));
	if(connection < 0){
		error("\nError connecting!\n");
	}

	strcpy(request, argv[3]);

	if(write(sock, request, strlen(request)) < 0){
	 	error("Error writing to socket");
	}

	char buffer[BUFSIZE];
	char Ack[2];
	char newfile[50]="copy";
	strcat(newfile,argv[3]);
	FILE* fp = fopen(newfile, "w+");
	srand(time(NULL));

	//Main loop, it ends when receiving all the packets.
	while (finish == 0) {
		memset(buffer,'\0',BUFSIZE);
		memset(Ack, '\0', 2);
		recvfrom(sock, buffer, BUFSIZE, 0, 0, 0);
		finish = buffer[1] - '0';
		currPack++;

		//Without loss probability
		if (argc == 4) {
			if(packCount < currPack) {
				packCount++;
			}
			printf("Packet %d accepted in receiver.\n", currPack);
			RegularSend(buffer, fp);
		}
		//With loss probability
		else {
			random = (float)rand() / (float)RAND_MAX;
			//Packet not lost
			if(random > loss) {
				printf("Random number: %f\n", random);
				if(packCount < currPack) {
					packCount++;
				}
				printf("Packet %d accepted in receiver.\n", currPack);
				if (RDT == 1) {
					RDTSend(buffer, &expected, Ack, &currPack, fp, &sock, &packCount);
				} else if (RDT == 0) {
					RegularSend(buffer, fp);
				}
				expected = (expected + 1) % 2;
			}
			//Packet lost
			else {
				printf("Random number: %f\n", random);
				printf("Packet %d dropped in receiver.\n", currPack);
				currPack--;
				if (RDT == 0) {
					if (buffer[1] == '1') {
						finish = 1;
						printf("Quit\n");
						currPack--;
					}
					currPack++;
				}
				else if(finish == 1){
					finish = 0;
				}
			}
		}

	}
	if (packCount == currPack) {
		printf("Received %d out of %d packets. Success.\n", packCount, currPack);
	} else {
		printf("Received %d out of %d packets. Fail.\n", packCount, currPack);
	}

	fclose(fp);
	close(sock);
	return 0;

}

void error(char *msg){
    perror(msg);
    exit(1);
}

void RDTSend(char buffer[], int* expected, char Ack[], int* currPack, FILE* fp, int* sock) {
	int i;
	float random;
	if (buffer[0] - '0' == *expected) {
		printf("Received expected packet in receiver.\n");
		for (i = 2; i < strlen(buffer); i++) {
			putc(buffer[i], fp);
		}
		Ack[0] = *expected + '0';
		//Sends Ack0 to server
		random = (float)rand() / (float)RAND_MAX;
		if (random > loss){
			printf("Sucessfully sent Ack for expected packet receipt\n");
			if(write(*sock, Ack, strlen(Ack)) < 0){
			 	error("Error writing to socket");
			}
	 	} else {
	 		printf("Failed to send ACK for expected packet.\n");
	 	}
	} else {
		printf("Received unexpected packet in receiver.\n");
		Ack[0] = ((*expected + 1) % 2) + '0';
		//Sends Ack1 to server
		random = (float)rand() / (float)RAND_MAX;
		if(random > loss){
			printf("Succesfully sent ACK for signifying duplicate packet receipt\n");
			if(write(*sock, Ack, strlen(Ack)) < 0){
			 	error("Error writing to socket");
			}
		}else{
			printf("Failed to send Ack for unexpected packet.\n");
		}
		*expected = (*expected +1) % 2;
		*currPack = *currPack - 1;
	}
}


void RegularSend(char buffer[], FILE* fp) {
	int i;
	for (i = 2; i < strlen(buffer); i++) {
				putc(buffer[i], fp);
			}
}
