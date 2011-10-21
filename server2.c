#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <fcntl.h>
#include <ctype.h>


void sigchld_handler(int s)
{
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

void serveClient(int);
int parseRequest(char requestBuffer[], int* fileFd, char** contentType,
		 int* httpVer, int* persistent);
char* contentTypeFinder(char* fileName);
int sendResponse(int sock, int fileFd, const int httpVer,
		 const int persistent, const char* contentType);
int sendBadResponse(int sock);
void error(char *msg)
{
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  struct sigaction sa;          // for signal SIGCHLD

  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
     
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
	   sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
     
  listen(sockfd,5);
     
  clilen = sizeof(cli_addr);
     
  /****** Kill Zombie Processes ******/
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  /*********************************/
 //newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  //printf ("creating a new socket!\n");
  while (1) {
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    printf ("creating a new socket!\n");
         
    if (newsockfd < 0) 
      error("ERROR on accept");
         
    pid = fork(); //create a new process
    if (pid < 0)
      error("ERROR on fork");
         
    if (pid == 0)  { // fork() returns a value of 0 to the child process
      close(sockfd);
      serveClient(newsockfd);
      close(newsockfd);
      exit(0);
    }
    else //returns the process ID of the child process to the parent
	continue;
    //  close(newsockfd); // parent doesn't need this 
  } /* end of while */
  return 0; /* we never get here */
}

// serves content for each client, maintaining a persistent connection
void serveClient (int sock)
{
  int nr, httpVer, persistent, parseSuccess, fileSuccess, contentTypeSuccess, fileFd;
  char* contentType;
  char requestBuffer[4000];
  
  //keeps connection open if Connection = keep-alive
  persistent = 1;
    do {
    //read the http request and print it out
    bzero(requestBuffer,4000);
    nr = read(sock,requestBuffer,4000);
    if (nr < 0) 
	error("ERROR reading from socket");
    printf("%s \n",requestBuffer);
    contentType = NULL;
   parseSuccess = parseRequest(requestBuffer, &fileFd, &contentType,
				&httpVer, &persistent);
    if (parseSuccess == 0){
      sendResponse(sock, fileFd, httpVer, persistent, contentType);
      close(fileFd);
    } else {
      sendBadResponse(sock);
    }
    if (contentType != NULL) {
      free(contentType);
    }
      } while (persistent == 1);
}

// parses through the http request
// connection should be persistent
// httpVer is 0 for 1.0, and 1 for 1.1
// returns 0 on success, -1 on failure
int parseRequest(char requestBuffer[], int* fileFd, char** contentType,
		 int* httpVer, int* persistent) {
  int fileNameLen;
  char* fileName;
  char* fileNameEnd;
  char* httpStart;
  
  if ( strncmp( requestBuffer, "GET ", 4 ) != 0 ) {
    // only supports GET requests. Error for anything else
    return -1;
  }

  //move to the file name
  requestBuffer += 4;
  while (requestBuffer[0] && (isspace(requestBuffer[0]) || requestBuffer[0] == '/')) {
    requestBuffer++;
  }
  //allocate memory for the file name
  fileNameEnd = strchr(requestBuffer, ' ');
  if (fileNameEnd == NULL) {
    return -1;
  }
  fileNameLen = fileNameEnd - requestBuffer;
  fileName = malloc((fileNameLen + 1) * sizeof(char));
  if (fileName == NULL) {
    return -1;
  }
  //copy over file name
  strncpy(fileName, requestBuffer, fileNameLen);
  fileName[fileNameLen + 1] = 0;

  //open file
  *fileFd = open(fileName, O_RDONLY);
  if (*fileFd == -1) {
    return -1;
  }

  // find the content type
  *contentType = contentTypeFinder(fileName);
  free(fileName);
  if (*contentType == NULL) {
    return -1;
  }
  // get http version
  if (strstr(requestBuffer, "HTTP/1.0") != NULL) {
    *httpVer = 0;
  } else if (strstr(requestBuffer, "HTTP/1.1") != NULL) {
    *httpVer = 1;
  } else {
    return -1;
  }

  // get if the connection should be persistent
  if (strstr(requestBuffer, "Connection: keep-alive")) {
    *persistent = 1;
  } else {
    *persistent = 0;
  }
  
  return 0;
}

// returns the content type if the file requested is supported
// returns NULL otherwise
char* contentTypeFinder(char* fileName) {
  char* contentType;
  int i;
  //capitalizing the filename to case insensitive search
  for (i = 0; i < strlen(fileName); i++) {
    fileName[i] = toupper(fileName[i]);
  }
  if (strstr(fileName, ".HTM")) {
    contentType = malloc(10 * sizeof(char));
    return strcpy(contentType, "text/html");
  } else if (strstr(fileName, ".JP")) {
    contentType = malloc(11 * sizeof(char));
    return strcpy(contentType, "image/jpeg" );
  } else if (strstr(fileName, ".GIF")) {
    contentType = malloc(10 * sizeof(char));
    return strcpy(contentType, "image/gif" );
  } else if (strstr(fileName, ".ICO")) {
    contentType = malloc(13 * sizeof(char));
    return strcpy(contentType, "image/x-icon");
  } else if (strstr(fileName, ".PNG")) {
    contentType = malloc(10 * sizeof(char));
    return strcpy(contentType, "image/png");
  } else if (strstr(fileName, ".BMP")) {
    contentType = malloc(10 * sizeof(char));
    return strcpy(contentType, "image/bmp");
  } else {
    return NULL;
  }
}

int sendResponse(int sock, int fileFd, const int httpVer,
		 const int persistent, const char* contentType) {
  char buffer[1024];
  char* headerPosPtr;
  char* filePosPtr;
  char *connection;
  int nr, nw, stringlen;
  // format the http header
  if (persistent == 1) {
    connection = "keep-alive";
  } else {
    connection = "close";
  }
  sprintf(buffer, "HTTP/1.%i 200 OK\r\nContent-Type: %s\r\nConnection: %s\r\n\r\n",
	  httpVer, contentType, connection);
  
  // send the http header
  nw = 0;
  stringlen = strlen(buffer);
  headerPosPtr = buffer;
  while (nw < stringlen) {
    stringlen -= nw;
    headerPosPtr += nw;
    nw = write(sock, headerPosPtr, stringlen);
    if (nw == -1) {
      error("ERROR writing to socket");
    }
  }

  // send the file
  while ((nr = read(fileFd, buffer, 1024)) > 0) {
    nw = 0;
    filePosPtr = buffer;
    while (nw < nr) {
      nr -= nw;
      filePosPtr += nw;
      nw = write(sock, filePosPtr, nr);
      if (nw == -1) {
	error("ERROR writing to socket");
      }
    }
  }
}

int sendBadResponse(int sock) {
  char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
  char* headerPosPtr;
  int nr, nw, stringlen;

  // send the http header
  nw = 0;
  stringlen = strlen(response);
  headerPosPtr = response;
  while (nw < stringlen) {
    stringlen -= nw;
    headerPosPtr += nw;
    nw = write(sock, headerPosPtr, stringlen);
    if (nw == -1) {
      error("ERROR writing to socket");
    }
  }
}
  
