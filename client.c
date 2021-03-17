#include<errno.h>
#include<sys/socket.h>
#include<stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include<stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h>

//Defining SBCP message header
struct sbcp_msg_header
{
  int vrsn;
  int type;
  int length;
};

//Defining SBCP Attribute
struct sbcp_attr
{
  int type;
  int length;
  char payload[512];
};

//Defining SBCP message
struct sbcp_msg
{
  struct sbcp_msg_header msg_header;
  struct sbcp_attr attr;
};

//Defining JOIN fucntion
int join(char *pl, int c_sock)
{
  char packet[534];
  memset(packet,'\0',sizeof(packet));
  int pl_len;
  pl_len = strlen(pl);
  
  struct sbcp_msg msg;
  msg.msg_header.vrsn = 3;
  msg.msg_header.type = 2; //message type for join is 2
  msg.attr.type = 2; //attr type for username is 2
  msg.attr.length = 2+2+pl_len;  //2bytes type, 2bytes length, length of attr payload
  
  int i;
  for(i=0;i<pl_len;i++)
    {
    msg.attr.payload[i]=pl[i];
    }
  msg.msg_header.length = 4+ 4 + pl_len; //4bytes versn type length,4bytes sbcp_attr type length, pl
  
  sprintf(packet, "%d:%d:%d:%d:%d:%s", msg.msg_header.vrsn, msg.msg_header.type, msg.msg_header.length, msg.attr.type, msg.attr.length, msg.attr.payload); 
  
  int sent = send(c_sock, packet, sizeof(packet), 0); //send message to the socket
  if (sent==-1)
    {
      puts("Failed to send JOIN message to the server.\n");
      exit(-1);
    }
  puts("JOIN message has been sent successfully to the server.\n");
  return sent;
  
}

//Defining SEND MESSAGE fucntion
int send_msg(int c_sock, char *uname) 
{
  char packet[534];
  char msg_send[512];
  memset(packet,'\0',sizeof(packet));
  memset(msg_send,'\0',sizeof(msg_send));
  fgets(msg_send,sizeof(msg_send),stdin);
  
  int msg_len;	
  msg_len = strlen(msg_send); 
  	
  struct sbcp_msg msg;
  msg.msg_header.vrsn = 3;
  msg.msg_header.type = 4; //message type for send is 4
  msg.attr.type = 4; //attr type for message is 2
  msg.attr.length = 2+2+msg_len;  //2bytes type, 2bytes length, length of attr payload
  //memset(msg.attr.payload,'\0',sizeof(msg.attr.payload));
  int i;
  for(i=0;i<msg_len;i++)
    {
    msg.attr.payload[i]=msg_send[i];
    }
    
  msg.msg_header.length = 4+ 4 + msg_len;//4bytes versn type length,4bytes sbcp_attr type length, pl
  
  //char sbcp_msg_send[1000];
  sprintf(packet, "%d:%d:%d:%d:%d:%s", msg.msg_header.vrsn, msg.msg_header.type, msg.msg_header.length, msg.attr.type, msg.attr.length, msg.attr.payload); 
  
  int sent = send(c_sock, packet, sizeof(packet), 0); //send message to the socket
  if (sent == -1)
    {
      puts("\nFailed to send message to the server.\n");
      return 0;
    }
  
  //puts("Message sent!");
  return sent;
}


//Defining RECV MESSAGE function
struct sbcp_msg recv_msg(char *recv_buff, int c, int readBytes)
{
  struct sbcp_msg msg;
  int rmsg_header_vrsn; 
  int rmsg_header_type;
  int rmsg_header_len;
  int rmsg_attr_type;
  int rmsg_attr_len;
  char rmsg_attr_payload[511];
  //printf("raw: %s",recv_buff);
  //Retrieve SBCP fields:
	int h;
	int field;
	field = 0;
	char translate[readBytes];
	
	for (h = 0; h < readBytes; h++){
		if (field == 0){
			if (recv_buff[h] == ':'){ //header version
				msg.msg_header.vrsn = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{				
				strncat(translate, &recv_buff[h], 1);
			}
		}
		else if (field == 1){
			if (recv_buff[h] == ':'){ //header type
				msg.msg_header.type = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &recv_buff[h], 1);
			}			
		}
		else if (field == 2){
			if (recv_buff[h] == ':'){ //header length
				msg.msg_header.length = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &recv_buff[h], 1);
			}	
		}
		else if (field == 3){
			if (recv_buff[h] == ':'){ //attribute type
				msg.attr.type = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &recv_buff[h], 1);
			}	
		}
		else if (field == 4){
			if (recv_buff[h] == ':'){ //attribute length
				msg.attr.length = atoi(translate);
				memset(&translate, '\0', sizeof(translate)/sizeof(char));
				field++;
			}
			else{
				strncat(translate, &recv_buff[h], 1);
			}	
		}
		else if (field == 5){
			strncat(translate, &recv_buff[h], 1);
		}
	}
	strcpy(msg.attr.payload, translate); //attribute payload	
  
  if (msg.attr.type == 3){ 
    //Number of clients
    printf("\nNumber of clients in the chat room: %s\n", msg.attr.payload);
    printf("Names of clients in the chat room: ");
    int p;
    for(p=1; p<=atoi(msg.attr.payload); p++)
    {
      //printf("%d",p);
      char recv_buf[534]; 
      int num = recv(c,recv_buf,534,0);
      recv_msg(recv_buf, c, num);
    }
    printf("\n");
    
  }
  
  if (msg.attr.type == 2){ 
    //Usernames
    printf("%s ", msg.attr.payload);
    //printf("Names of clients in the chat room: %s\n", msg.attr.payload);
  }
  
  if (msg.attr.type == 4){ 
    //Message
    char recv_buf[534]; 
    int num = recv(c,recv_buf,534,0);
    struct sbcp_msg ret_pack;
    ret_pack = recv_msg(recv_buf, c, num);
    printf(": %s\n", msg.attr.payload);
    
    
  }
  
  if (msg.attr.type == 1){ 
    //Reason for failure
    if (strcmp(msg.attr.payload,"Abrupt Exit") == 0)
    {
      char recv_buf[534]; 
      int num = recv(c,recv_buf,534,0);
      struct sbcp_msg ret_pack;
      ret_pack = recv_msg(recv_buf, c, num);
      printf(" has left the group chat!\n");
    }
    
    else if (strcmp(msg.attr.payload,"Chat Room Full") == 0)
    {
      printf("%s ", msg.attr.payload);
      close(c);
      exit(-1);
    }
    
    else
    { 
      printf("%s ", msg.attr.payload);
      close(c);
      exit(-1);
    }
  }
  
  //printf("Payloads: %s\n", msg.attr.payload);
  return msg;
}


int main(int argc,char* argv[])
{
   
   if (argc < 4){ //make sure command line sets proper number of arguments
		printf("Please specify client's username, server's IPv4 address and port number\n");
		return 0;
   }
   char uname[15];
   memset(uname,'\0',sizeof(uname));
   strcpy(uname,argv[1]);
   int ulen;
   ulen = strlen(uname); 
   
   if (ulen > 16){ //make sure length of username is not more than 16
		printf("Please make sure username is not more than 16 characters.\n");
		return 0;
   }
   
   //Creating a socket
   int c;
   c = socket(AF_INET, SOCK_STREAM, 0);
   if (c==-1)
   {
     perror("The socket could not be created.\n");
     exit(-1);
   }
   puts("Socket Created.\n");
   
   char server_ip;
   int pnum; 
   
   struct sockaddr_in serveraddr;
   memset(&serveraddr,'\0',sizeof(serveraddr)); 
   serveraddr.sin_family = AF_INET;
   serveraddr.sin_port = htons(atoi(argv[3]));
   serveraddr.sin_addr.s_addr = inet_addr(argv[2]);
   
   //Conecting to server
   if (connect(c, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1)
   {
     perror("Failed to connect to server");
     exit(-1);
   }
   puts("Connected to server. \n");
   
   int j_bytes = join(uname, c); 
   char recv_buf[534];  
   
   //I/O Multiplexing
   int max_fd;
   fd_set readfds; 
   FD_ZERO(&readfds);
   
   while(1)
   {
     FD_SET(0,&readfds);  //For stdin input in readfds set
     FD_SET(c, &readfds);  //For socket in readfds set
   
     max_fd = c;
     int rv;
     //printf("Here");
     rv = select(max_fd+1, &readfds, NULL, NULL, NULL);
     if (rv == -1)
     {
       puts("Error in select.\n");
       exit(-1);
     }
     
     if (FD_ISSET(0,&readfds))
     {
       send_msg(c,uname);
     }
     
     else if (FD_ISSET(c,&readfds))
     {
       int num = recv(c,recv_buf,534,0);
       recv_msg(recv_buf, c, num);
     }
     
   }

   
   //CLOSING CONNECTION
   puts("Client closing connection");
   close(c);
   return 0;
}
  

  