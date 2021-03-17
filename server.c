#include<sys/socket.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netinet/in.h>
#include<errno.h>
#include<arpa/inet.h>

//SBCP Packet Structure:

	//Defining SBCP message header
struct sbcp_msg_header
{
  int vrsn; //3
  int type; // FWD (for server)
  int length; //SBCP message length
};

	//Defining SBCP Attribute
struct sbcp_attr
{
  int type; //2 (username), 4 (message), 1 (Reason), or 3 (Client)
  int length; //length of SBCP attribute
  char payload[512]; //largest attribute type (message)
};

	//Defining SBCP message
struct sbcp_msg
{
  struct sbcp_msg_header msg_header;
  struct sbcp_attr attr;
}; 

//Written Function (returns number of bytes written or -1 for error): (FWD)
int written(int descriptor, struct sbcp_msg data){
	char packet[534]; //SBCP data packet
	memset(packet,'\0',sizeof(packet));
	
	//compress SBCP data into packet of type char: 
	sprintf(packet, "%d:%d:%d:%d:%d:%s", data.msg_header.vrsn, data.msg_header.type, data.msg_header.length, data.attr.type, data.attr.length, data.attr.payload); 	
	int sent = send(descriptor, packet, 534, 0); //send message to the socket
	
	if(sent == -1){ //if error in sending, return 0 bytes sent
	
		if (errno == EINTR){ //if process is interrupted, resend it again:
		
			printf("Server: Write Interrupted. Rewriting Now.\n");
			written(descriptor, data);
		}
		
		perror("Server: Write Error");		
		return -1;
	}
	
	return sent; //if transmitting message is successful, return number of bytes sent
}

//Reading Function lets server read data received from client: (JOIN or SEND)
struct sbcp_msg reading(int descriptor, char data[], int dataLength, int *byteRD){ ///////////////////////////agree on the lengths??
	
	int readBytes = recv(descriptor, data, dataLength, 0); //reads message from client
	*byteRD = readBytes; //assign variable pointed by this pointer the number of bytes read
	struct sbcp_msg packet; //temporary packet holder
	
	//Retrieve SBCP fields:
	int h;
	int field;
	field = 0;
	char translate[readBytes];
	
	for (h = 0; h < readBytes; h++){
		if (field == 0){
			if (data[h] == ':'){ //header version
				packet.msg_header.vrsn = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{				
				strncat(translate, &data[h], 1);
			}
		}
		else if (field == 1){
			if (data[h] == ':'){ //header type
				packet.msg_header.type = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &data[h], 1);
			}			
		}
		else if (field == 2){
			if (data[h] == ':'){ //header length
				packet.msg_header.length = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &data[h], 1);
			}	
		}
		else if (field == 3){
			if (data[h] == ':'){ //attribute type
				packet.attr.type = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &data[h], 1);
			}	
		}
		else if (field == 4){
			if (data[h] == ':'){ //attribute length
				packet.attr.length = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &data[h], 1);
			}	
		}
		else if (field == 5){
			strncat(translate, &data[h], 1);
		}
	}
	strcpy(packet.attr.payload, translate); //attribute payload	
	
	if (packet.attr.length > 4){
		int g; ///////////////////////////////////////////////////////////////////////////////////////agree on lengths
		char tmp[packet.attr.length - 4];
		memset(&tmp, '\0', sizeof(tmp)); 
		for (g = 0; g < packet.attr.length - 4; g++){
			strncat(tmp, &packet.attr.payload[g], 1);
		}
		memset(&packet.attr.payload, '\0', sizeof(packet.attr.payload)); 
		strcpy(packet.attr.payload, tmp);
	}
	
	if (readBytes == -1){ //if recv() returns -1, there is an error
		perror("Server: Read error so read again");		
	}
	
	return packet; //return the SBCP packet
}

int main(int argc, char *argv[]){ //command line: echos port (echos is name of program, port is port number)

	//Declare parameters/variables used:
	int sckt; //socket descriptor
	struct sockaddr_in my_addr; //server socket address
	struct sockaddr_in nxt_client; //client socket address wanting to connect in port
	int new_sckt; //next socket descriptor to connect to client
	char msg[1024]; //message to be sent
	char emp[1024]; memset(&emp,'\0', sizeof(emp)); //comparator to detect "no text" from client (detects whitespace)
	char emp1[1024]; memset(&emp1,'\0', sizeof(emp1)); //comparator to detect "no text" from client (detects tab character)
	int msgLength; //length of the message recieved by server
	int num_bytes; //number of bytes written to socket using written()
	int pid; //child process identification
	int sizeAddr = sizeof(struct sockaddr_in); //size of socket address
	
	int yes = 1;
	
	fd_set master; // list of master file descriptors
	fd_set temp; // list of temporary file descriptors for select()
	
	int fd_max; //maximum file descriptor number
	int timeout = 10; //timeout for waiting for I/O in select()
	int ready_fd; 
	int max_clients = atoi(argv[3]); //maximum number of clients for chat room
	int num_clients = 0; //number of clients currently in chat room
	char usernames [max_clients][16]; //array of usernames of clients
	int trace_id[max_clients]; //keeps track of index of each socket/fd
	struct sbcp_msg SBCP; //SBCP data packet
	char usr_nm[16]; //current username of current client
	char err_msg[1024]; //error message to be sent
	struct sbcp_msg SBCP_cpy; //to copy SBCP packets
	
	//initialize sets of file descriptors:
	FD_ZERO(&master);
	FD_ZERO(&temp);
    
	//Establish socket descriptor:
	if ((sckt = socket(AF_INET, SOCK_STREAM, 0)) == -1){ //assign socket descriptor
		perror("Server: socket"); // handling error
		exit(-1);
	}
	
	if (setsockopt(sckt, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){ //reuse port if address is already in use
		perror("Server: setsockopt"); // handling error
		exit(-1);
	}
	
	//Get information from socket address:
	my_addr.sin_family = AF_INET; //host byte order
	
	if (argc < 2){ //make sure command line sets proper number of arguments
		printf("Please specify the IP address, port number and maximum clients allowed\n");
		return 0;
	}
	else{
		my_addr.sin_port = htons(atoi(argv[2])); // short, network byte order (atoi to convert input argument from char to integer)
	}
	
	//inet_aton(argv[1], &(my_addr.sin_addr));

	my_addr.sin_addr.s_addr = inet_addr(argv[1]); //assign server's IP with specified one in command line
	
	memset(&my_addr.sin_zero, '\0', 8); //clean lingering bytes used previously in address
	
	printf("Server: Starting connection to socket\n");
	
	//Bind:
	if (bind(sckt, (struct sockaddr*) &my_addr, sizeAddr) == -1){
		perror("Server: Bind Error");
		exit(-1);
	}
	
	//Listen:
	if (listen(sckt, max_clients) == -1){
		perror("Server: Listen Error");
		exit(-1);
	}
	
	printf("Server: Listening in port %s\n", argv[2]);
	
	printf("Server: Maximum number of clients is %d\n", max_clients);
	
	FD_SET(sckt, &master); //add listening socket to master set
	
	fd_max = sckt; //set maximum file descriptor number to listening socket value
	
	//intialize usernames in chat room as nothing:
	int q;
	for(q = 0; q <= max_clients; q++){
		usernames[q][0] = '\0';
		trace_id[q] = sckt;
	}
	
	int fnum = 0; //current file descriptor
	
	//Accept thread:
	while(1){
		FD_ZERO(&temp);
		temp = master; //update from master set
		
		ready_fd = select(fd_max+1, &temp, NULL, NULL, NULL); //select() waits for multiple inputs/outputs from clients
		if (ready_fd == -1){
			perror("Server: Select Error");
			exit(1);
		}
		
		for (fnum = 0; fnum <= fd_max; fnum++){
			if(FD_ISSET(fnum, &temp)){
				if(fnum == sckt){ //if wanting to connect, JOIN:	
				
					//accept clients wanting to connect:			
					if ((new_sckt = accept(sckt, (struct sockaddr*) &nxt_client, &sizeAddr)) == -1){
						perror("Server: Accept Error");
						continue;
					}		

					//accept only if max number of clients is not reached:
					if (abs(max_clients-num_clients) != 0) {
						
						//get JOIN message from client:	
						
							//get username of client:
						memset(&msg, '\0', sizeof(msg)); //initialize username to be empty
						SBCP = reading(new_sckt, msg, 1024, &msgLength); //read username from client
						
						if ((&msg[msgLength-1] == NULL) || (msgLength <= 0)){ //if last character inputed indicates EOF, it means client is disconnected
							printf("Server: client wanting to connect in socket %d failed to connect\n", new_sckt);
							close(new_sckt); //close socket
						}
						
						//check if SBCP is JOIN message and username attribute:
						if (SBCP.attr.type == 2 && SBCP.msg_header.type == 2 && msgLength != -1){
							strncpy(usr_nm, SBCP.attr.payload, 15);
							usr_nm[SBCP.attr.length] = '\0'; //clear remaining whitespace
						}
						else{ //Reason of Failure (no JOIN message recieved)
							//FWD reason of failure to connect:
							memset(&err_msg, '\0', sizeof(err_msg)); //initialize message to be empty
							strcpy(err_msg, "JOIN MSG Failed");	//reason of failure	
							
								//FWD reason of failure:
							SBCP.msg_header.vrsn = 3;
							SBCP.msg_header.type = 3; //message type for FWD is 3
							SBCP.attr.type = 1; //attr type for reason of failure is 1
							SBCP.attr.length = 15;  //2bytes type, 2bytes length, length of attr payload
							memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
							strcpy(SBCP.attr.payload, err_msg);

							SBCP.msg_header.length = 15 + 15;
							
							num_bytes = written(new_sckt, SBCP); //write back the same message to client
							
							//Check for error in writing message to socket:
							if (num_bytes == -1){ 
								printf("Server: Error in forwarding message\n");
							}
							
							memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message buffer
							close(new_sckt); //Disconnect
							printf("Server: Not recieved JOIN message from client in socket %d\n", new_sckt);
							continue;
						}

							//compare this username from the usernames in chat room:
						int in_use = 0;
						int k;
						for (k=0; k < num_clients; k++){
							if (strcmp(usr_nm,usernames[k]) == 0){
								in_use = 1;
							}			
						}
						
							//if username already in use, tell client it is already in use
						if(in_use){ 
							memset(&err_msg, '\0', sizeof(err_msg)); //initialize message to be empty
							strcpy(err_msg, "Username Taken");	//reason of failure	
							
								//FWD reason of failure:
							SBCP.msg_header.vrsn = 3;
							SBCP.msg_header.type = 3; //message type for FWD is 3
							SBCP.attr.type = 1; //attr type for reason of failure is 1
							SBCP.attr.length = 14;  //2bytes type, 2bytes length, length of attr payload
							memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
							strcpy(SBCP.attr.payload, err_msg);

							SBCP.msg_header.length = 15 + 14;
							
							num_bytes = written(new_sckt, SBCP); //write back the same message to client
							
							//Check for error in writing message to socket:
							if (num_bytes == -1){ 
								printf("Server: Error in forwarding message\n");
							}
							printf("Server: Username already in use. Client in scoket %d will try to another username\n", new_sckt);
							close(new_sckt); //Disconnect
							memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message buffer
							continue;
						}
							
							//add new username to chat room:
						strcpy(usernames[num_clients], usr_nm);
							//keep track of which client/socket has what username: 
						trace_id[num_clients] = new_sckt;
						
						printf("Server: Accepted %s client's request to communicate at socket %d\n", usernames[num_clients], new_sckt);
						
						FD_SET(new_sckt, &master); //push in new socket to master set
						num_clients++; //increment number of clients
						
						//update maximum value of file descriptor in master set:
						if (new_sckt > fd_max){ 
							fd_max = new_sckt;
						}
						
						//FWD ACK to client:
						
							//Number of clients in chat room:
								//SBCP attribute 'Client Count':
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 3; //attr type for client count is 3
						SBCP.attr.length = (sizeof(num_clients)/sizeof(int));  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						sprintf(SBCP.attr.payload, "%d", num_clients); 

						SBCP.msg_header.length = 15 + (sizeof(num_clients)/sizeof(int));
					
						num_bytes = written(new_sckt, SBCP); //write back the same message to client
						
						//Check for error in writing message to socket:
						if (num_bytes == -1){ 
							printf("Server: Error in forwarding message\n");
						}
								
							//Usernames of clients in chat room:
								//SBCP attribute 'Username':
						int n;
						for(n = 0; n < num_clients; n++){
							SBCP.msg_header.vrsn = 3;
							SBCP.msg_header.type = 3; //message type for FWD is 3
							SBCP.attr.type = 2; //attr type for username is 2
							SBCP.attr.length = (sizeof(usernames[n])/sizeof(char));  //2bytes type, 2bytes length, length of attr payload
							memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
							strcpy(SBCP.attr.payload, usernames[n]);

							SBCP.msg_header.length = 15 + (sizeof(usernames[n])/sizeof(char));
							num_bytes = written(new_sckt, SBCP); //write back the same message to client
							
							//Check for error in writing message to socket:
							if (num_bytes == -1){ 
								printf("Server: Error in forwarding message\n");
							}
						}
								
					}
					else{ //if max number of clients is reached, close/recycle socket
					
						//FWD reason of failure to connect:
						memset(&err_msg, '\0', sizeof(err_msg)); //initialize message to be empty
						strcpy(err_msg, "Chat Room Full");	//reason of failure	
						
							//FWD reason of failure:
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 1; //attr type for reason of failure is 1
						SBCP.attr.length = 14;  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						strcpy(SBCP.attr.payload, err_msg);

						SBCP.msg_header.length = 15 + 14;
						
						num_bytes = written(new_sckt, SBCP); //write back the same message to client
						
						//Check for error in writing message to socket:
						if (num_bytes == -1){ 
							printf("Server: Error in forwarding message\n");
						}
						
						memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message buffer
						
						close(new_sckt); //Disconnect
						printf("Server: Chat room is full\n");
					}
				}
				else{ //SEND AND FWD
							
					//Find username of this client:
					int z;
					int indices;
					char username[16];
					for (z = 0; z < num_clients; z++){
						if (fnum == trace_id[z]){
							strcpy(username, usernames[z]);
							indices = z;
							break;
						}
					}		
					
					//Read message SEND by client:
					memset(&msg, '\0', sizeof(msg)); //initialize message to be empty
					
						//process SBCP SEND() from client:
					SBCP = reading(fnum, msg, 1024, &msgLength); //read username from client
					
					
					//check if SBCP is SEND message and message attribute:
					if (SBCP.msg_header.type != 4 || msgLength == -1){
						//FWD reason of failure to connect:
						memset(&err_msg, '\0', sizeof(err_msg)); //initialize message to be empty
						strcpy(err_msg, "SEND MSG Failed");	//reason of failure	
						
							//FWD reason of failure:
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 1; //attr type for reason of failure is 1
						SBCP.attr.length = 15;  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						strcpy(SBCP.attr.payload, err_msg);

						SBCP.msg_header.length = 15 + 15;
						
						num_bytes = written(new_sckt, SBCP); //write back the same message to client
						
						//Check for error in writing message to socket:
						if (num_bytes == -1){ 
							printf("Server: Error in forwarding message\n");
						}
						
						memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message buffer
						printf("Server: Not recieved SEND message from client in socket %d\n", new_sckt);
					}
					
					if ((&msg[msgLength-1] == NULL) || (msgLength <= 0)){ //if last character inputed indicates EOF, it means client is disconnected
						printf("Server: %s in socket %d left chat\n", username, fnum);
						
						//broadcast exit of client:
						memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message
						strcpy(err_msg, "Abrupt Exit");	//reason of failure					
						int i;
						for (i = 0; i <= fd_max; i++){
							if (FD_ISSET(i, &master) && i != sckt && i != fnum){								
								//FWD reason of failure message with the username in 2 packets:
								
									//FWD reason of failure:
								SBCP.msg_header.vrsn = 3;
								SBCP.msg_header.type = 3; //message type for FWD is 3
								SBCP.attr.type = 1; //attr type for reason of failure is 1
								SBCP.attr.length = 11;  //2bytes type, 2bytes length, length of attr payload
								memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
								strcpy(SBCP.attr.payload, err_msg);

								SBCP.msg_header.length = 15 + 11;
								
								num_bytes = written(i, SBCP); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}
								
									//FWD username:
								SBCP.msg_header.vrsn = 3;
								SBCP.msg_header.type = 3; //message type for FWD is 3
								SBCP.attr.type = 2; //attr type for username is 2
								SBCP.attr.length = (sizeof(username)/sizeof(char));  //2bytes type, 2bytes length, length of attr payload
								memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
								strcpy(SBCP.attr.payload, username);

								SBCP.msg_header.length = 15 + (sizeof(username)/sizeof(char));
								
								num_bytes = written(i, SBCP); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}
							}
						}
						memset(&err_msg, '\0', sizeof(err_msg)); //clear/reset message
						
						//recycle resources reserved for this client:
						close(fnum); //close socket
						FD_CLR(fnum, &master); //remove socket from master set
						num_clients--; //decrement number of clients
						
							//delete username:
						if (indices < max_clients -1){ //if not last client, delete from the middle and copy over the remaining clients:
								
								//copy over remianing usernames and their indices:
							int p;			
							for (p = indices; p < max_clients - 1; p++){
								strcpy(usernames[p], usernames[p+1]);
								trace_id[p] = trace_id[p+1];
							}
							
								//erase redudancy:
							int d;
							for (d = num_clients; d < max_clients; d++){
								memset(&usernames[d], '\0', sizeof(usernames[d])/sizeof(char)); 
								trace_id[d] = sckt;
							}
						}
						else{ //otherwise, just delete the last client
							memset(&usernames[indices], '\0', sizeof(usernames[indices])); 
							trace_id[indices] = sckt;
						}
						
						continue;
					}
					
					memset(&emp,' ', msgLength-1); //set comparator as whitespace of the same length of message except the termination ('\n')
					memset(&emp1,'\t', msgLength-1); //set comparator as tab of the same length of message except the termination ('\n')
					
					if ((strcmp(msg,emp) != 0) && (strcmp(msg,emp1) != 0)){ //do not echo when no text/character is sent (whitespace is not considered)				
						//copy SEND message packet
						SBCP_cpy = SBCP;
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						
						//create SBCP username packet:
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 2; //attr type for username is 2
						SBCP.attr.length = (sizeof(username)/sizeof(char));  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						strcpy(SBCP.attr.payload, username);

						SBCP.msg_header.length = 15 + (sizeof(username)/sizeof(char));
						
						int i;
						for (i = 0; i <= fd_max; i++){
							if (FD_ISSET(i, &master) && i != sckt && i != fnum){								
								//FWD message with the username in 2 packets:
								
									//FWD message packet:
								num_bytes = written(i, SBCP_cpy); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}
									
									//FWD username packet:
								num_bytes = written(i, SBCP); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}
								
								
							}
						}
					}
					else{
						strcpy(msg, '\0'); //if blank, send nothing 
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 4; //attr type for message is 2
						SBCP.attr.length = 0;  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						strcpy(SBCP.attr.payload, msg);
						SBCP.msg_header.length = 15;
						
						//copy SEND message packet
						SBCP_cpy = SBCP;
						
						//create SBCP username packet:
						SBCP.msg_header.vrsn = 3;
						SBCP.msg_header.type = 3; //message type for FWD is 3
						SBCP.attr.type = 2; //attr type for username is 2
						SBCP.attr.length = (sizeof(username)/sizeof(char));  //2bytes type, 2bytes length, length of attr payload
						memset(SBCP.attr.payload,'\0',sizeof(SBCP.attr.payload)/sizeof(char));
						strcpy(SBCP.attr.payload, username);

						SBCP.msg_header.length = 15 + (sizeof(username)/sizeof(char));
						
						int i;
						for (i = 0; i <= fd_max; i++){
							if (FD_ISSET(i, &master) && i != sckt && i != fnum){
								//FWD message with the username in 2 packets:
								
									//FWD message:								
								num_bytes = written(i, SBCP_cpy); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}		
								
									//FWD username:
								num_bytes = written(i, SBCP); //write back the same message to client
								
								//Check for error in writing message to socket:
								if (num_bytes == -1){ 
									printf("Server: Error in forwarding message\n");
								}						
							}
						}
					}
					
					memset(&msg, '\0', sizeof(msg)); //clear/reset message
					memset(&emp,'\0', sizeof(emp)); memset(&emp1,'\0', sizeof(emp1));//clear/reset comparators
				}
			}
		}
	}
	return 0;
}
