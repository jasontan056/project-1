/* 
   A simple server in the internet domain using TCP
   Usage:./server port (E.g. ./server 10000
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
 	int sockfd, newsockfd; //descriptors return from socket and accept system calls
     int portno; // port number
     socklen_t clilen;
     
     char buffer[4000];
     
     /*sockaddr_in: Structure Containing an Internet Address*/
     struct sockaddr_in serv_addr, cli_addr;
     
     int n;
     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     
     /*Create a new socket
       AF_INET: Address Domain is Internet 
       SOCK_STREAM: Socket Type is STREAM Socket */
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]); //atoi converts from String to Integer
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY; //for the server the IP address is always the address that the server is running on
     serv_addr.sin_port = htons(portno); //convert from host to network byte order
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) //Bind the socket to the server address
              error("ERROR on binding");
      for(;;){ //this infinite loop is so that we can do multiple requests on one socket and thus have pictures in html files
	     listen(sockfd,5); // Listen for socket connections. Backlog queue (connections to wait) is 5
	     
	     clilen = sizeof(cli_addr);
	     /*accept function: 
	       1) Block until a new connection is established
	       2) the new socket descriptor will be used for subsequent communication with the newly connected client.
	     */
	     newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	     if (newsockfd < 0) 
		  error("ERROR on accept");

	     bzero(buffer,4000);
	     n = read(newsockfd,buffer,4000); //Read is a block function. It will read at most 255 bytes
	     if (n < 0) error("ERROR reading from socket");
		printf("%s\n",buffer);
	/*************** This is a block of code to parse for the requested file *********/
		char * store_buffer = buffer;
		const char *http_tok = "HTTP";
		char * ptr;
		ptr = strstr(store_buffer, http_tok); //find the location of the HTTP word.
		const void * store_buffer_sans = (const void *)store_buffer+5; //Ptr to the request message after GET_/

		int get_length = strlen(store_buffer); //Get the length of the entire request message from 'GET' onward
		int http_length = strlen(ptr); //Get the length of the request message from 'HTTP' onward

		char file_name_buffer[get_length-http_length]; //Declaration for the buffer that will hold the requested file
		memcpy(file_name_buffer,store_buffer_sans,(get_length-http_length)); //copy the file name

		file_name_buffer[get_length-http_length-6] = '\0'; //this is was just trial and error, need to make sure the 6 is logical.

		printf("http length %i\n", http_length);
		printf("get length %i\n", get_length);
		printf("the html address is %s\n",file_name_buffer);

	/******************************End Parser****************************************************/
	     //testing out sending a picture!
	    // char pictureFileName[ 50 ] = "maru.jpg\0";
	     int pictureFd = open( file_name_buffer, O_RDONLY ); //changed this to receive my file_name_buffer
	     if ( pictureFd == -1 ) {
	       error( "Error on opening file" );
	     }
	     int read_count;
	     int write_count = 0;
	     char pictureBuf[ 1024 ];
	     char* bufferPtr;
	     while( ( read_count = read( pictureFd, pictureBuf, 1024 ) ) > 0 ) {
	       write_count = 0;
	       bufferPtr = pictureBuf;
	       while ( write_count < read_count ) {
		 read_count -= write_count;
		 bufferPtr += write_count;
		 write_count = write( newsockfd, bufferPtr, read_count );
		 if ( write_count == -1 ) {
		   error( "Socket read error" );
		 }
	       }
	     }     

	     //commented it out for now because the server was crashing for some reason
	/*      bzero(buffer,256); */
	/*      while( read(newsockfd,buffer,255) > 0) //added this loop to get all of the header info */
	/*      { */
	/*        //  n = read(newsockfd,buffer,255); //Read is a block function. It will read at most 255 bytes */
	/*        // if (n < 0) error("ERROR reading from socket"); */
	/*        printf("%s",buffer); */
	/*      } */
	     
	     n = write(newsockfd,"I got your message",18); //NOTE: write function returns the number of bytes actually sent out Ñ> this might be less than the number you told it to send
	     if (n < 0) error("ERROR writing to socket");
      }
     close(sockfd);
     close(newsockfd);
     return 0; 
}

