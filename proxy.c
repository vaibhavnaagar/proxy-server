

#include "proxy_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400

int port = 5100;									// Default Port
int socketId;										// Server Socket ID

pid_t client_PID[MAX_CLIENTS];						// PID of connected clients


int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}

	return 1;
}



int connectRemoteServer(char* host_addr, int port_num)
{
	// Creating Socket for remote server---------------------------

	int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

	if( remoteSocket < 0)
	{
		printf("Error in Creating Socket.\n");
		return -1;
	}
	
	// Get host by the name or ip address provided

	struct hostent *host = gethostbyname(host_addr);	
	if(host == NULL)
	{
		fprintf(stderr, "No such host exists.\n");	
		return -1;
	}

	// inserts ip address and port number of host in struct `server_addr`
	struct sockaddr_in server_addr;

	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_num);

	bcopy((char *)host->h_addr,(char *)&server_addr.sin_addr.s_addr,host->h_length);

	// Connect to Remote server ----------------------------------------------------

	if( connect(remoteSocket, (struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr)) < 0 )
	{
		fprintf(stderr, "Error in connecting !\n"); 
		return -1;
	}

	return remoteSocket;
}


int handleGETrequest(int clientSocket, ParsedRequest *request, char *buf)
{
	strcpy(buf, "GET ");
	strcat(buf, request->path);
	strcat(buf, " ");
	strcat(buf, request->version);
	strcat(buf, "\r\n");

	size_t len = strlen(buf);

	if (ParsedHeader_set(request, "Connection", "close") < 0){
		printf("set header key not work\n");
		//return -1;										// If this happens Still try to send request without header
	}

	if(ParsedHeader_get(request, "Host") == NULL)
	{
		if(ParsedHeader_set(request, "Host", request->host) < 0){
			printf("Set \"Host\" header key not working\n");
		}
	}

	if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0) {
		printf("unparse failed\n");
		//return -1;										// If this happens Still try to send request without header
	}


	int server_port = 80;									// Default Remote Server Port
	if(request->port != NULL)
		server_port = atoi(request->port);

	int remoteSocketID = connectRemoteServer(request->host, server_port);

	if(remoteSocketID < 0)
		return -1;

	int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);

	bzero(buf, MAX_BYTES);

	bytes_send = recv(remoteSocketID, buf, MAX_BYTES-1, 0);

	while(bytes_send > 0)
	{
		bytes_send = send(clientSocket, buf, bytes_send, 0);

		if(bytes_send < 0)
		{
			perror("Error in sending data to client socket.\n");
			break;
		}
			
		bzero(buf, MAX_BYTES);

		bytes_send = recv(remoteSocketID, buf, MAX_BYTES-1, 0);


	} 
	printf("Done\n");

	bzero(buf, MAX_BYTES);

	close(remoteSocketID);

	return 0;

}


int checkHTTPversion(char *msg)
{
	int version = -1;

	if(strncmp(msg, "HTTP/1.1", 8) == 0)
	{
		version = 1;
	}
	else if(strncmp(msg, "HTTP/1.0", 8) == 0)			
	{
		version = 1;										// Handling this similar to version 1.1
	}
	else
		version = -1;

	return version;
}



int requestType(char *msg)
{
	int type = -1;

	if(strncmp(msg, "GET\0",4) == 0)
		type = 1;
	else if(strncmp(msg, "POST\0",5) == 0)
		type = 2;
	else if(strncmp(msg, "HEAD\0",5) == 0)
		type = 3;
	else
		type = -1;

	return type;
}



void respondClient(int socket)
{

	int bytes_send, len;											// Bytes Transferred

										
	char *buffer = (char*)calloc(MAX_BYTES,sizeof(char));			// Creating buffer of 4kb for a client

	//bzero(buffer, MAX_BYTES);										// Make buffer zero

	bytes_send = recv(socket, buffer, MAX_BYTES, 0);				// Receive Request

	while(bytes_send > 0)
	{
		len = strlen(buffer);
		if(strstr(buffer, "\r\n\r\n") == NULL)
		{	
			//printf("Carriage Return Not found!\n");
			bytes_send = recv(socket, buffer + len, MAX_BYTES - len, 0);
		}
		else{
			break;
		}
	}

	if(bytes_send > 0)
	{
		//printf("%s\n",buffer);
		len = strlen(buffer); 

		//Create a ParsedRequest to use. This ParsedRequest
		//is dynamically allocated.
		ParsedRequest *req = ParsedRequest_create();

		if (ParsedRequest_parse(req, buffer, len) < 0) 
		{
			sendErrorMessage(socket, 500);									// 500 internal error
		   	printf("parse failed\n");
		}
		else
		{

			bzero(buffer, MAX_BYTES);

			int type = requestType(req->method);

			if(type == 1)											// GET Request
			{
				if( req->host && req->path && (checkHTTPversion(req->version) == 1) )
				{
					bytes_send = handleGETrequest(socket, req, buffer);		// Handle GET request
					if(bytes_send == -1)
					{	
						sendErrorMessage(socket, 500);
					}
				}

				else
					sendErrorMessage(socket, 500);					// 500 Internal Error

			}
			else if(type == 2)										// POST Request
			{
				printf("POST: Not implemented\n");
				sendErrorMessage(socket, 500);
			}
			else if(type == 3)										// HEAD Request
			{
				printf("HEAD: Not implemented\n");
				sendErrorMessage(socket, 500);
			}
			else													// Unknown Method Request
			{
				printf("Unknown Method: Not implemented\n");
				sendErrorMessage(socket, 500);
			}


		}

		ParsedRequest_destroy(req);

	}

	if( bytes_send < 0)
	{
		perror("Error in receiving from client.\n");
	}
	else if(bytes_send == 0)
	{
		printf("Client disconnected!\n");
	}

	shutdown(socket, SHUT_RDWR);
	close(socket);													// Close socket
	free(buffer);
	return;
}



int findAvailableChild(int i)
{
	int j = i;
	pid_t ret_pid;
	int child_state;

	do
	{
		if(client_PID[j] == 0)
			return j;
		else
		{
			ret_pid = waitpid(client_PID[j], &child_state, WNOHANG);		// Finds status change of pid

			if(ret_pid == client_PID[j])									// Child exited
			{
				client_PID[j] = 0;
				return j;
			}
			else if(ret_pid == 0)											// Child is still running
			{
				;
			}
			else
				perror("Error in waitpid call\n");
		}
		j = (j+1)%MAX_CLIENTS;
	}
	while(j != i);

	return -1;
}



int main(int argc, char * argv[]) {

	int newSocket, client_len;

	struct sockaddr_in server_addr, client_addr;

	bzero(client_PID, MAX_CLIENTS);

	// Fetching Arguments----------------------------------------------------------------------------------
	int params = 1;

	if(argc == 2)
	{
		port = atoi(argv[params]);
	}
	else
	{
		printf("Wrong Arguments! Usage: %s <port-number>\n", argv[0]);
		exit(1);
	}

	printf("Setting Proxy Server Port : %d\n", port);


	// Creating socket-------------------------------------------------------------------------------------

	socketId = socket(AF_INET, SOCK_STREAM, 0);

	if( socketId < 0)
	{
		perror("Error in Creating Socket.\n");
		exit(1);
	}

	int reuse =1;
	if (setsockopt(socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEADDR) failed\n");

	//----------------------------------------------------------------------------------------------------

	// Binding socket with given port number and server is set to connect with any ip address-------------

	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if( bind(socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
	{
		perror("Binding Error : Port may not be free. Try Using diffrent port number.\n");
		exit(1);
	}

	printf("Binding successful on port: %d\n",port);

	//-----------------------------------------------------------------------------------------------------

	// Listening for connections and accept upto MAX_CLIENTS in queue--------------------------------------

	int status = listen(socketId, MAX_CLIENTS);

	if(status < 0 )
	{
		perror("Error in Listening !\n");
		exit(1);
	}

	//-----------------------------------------------------------------------------------------------------

	// Infinite Loop for accepting connections-------------------------------------------------------------

	int i=0;
	int ret;

	while(1)
	{
		//printf("Listening for a client to connect!\n");
		bzero((char*)&client_addr, sizeof(client_addr));							// Clears struct client_addr
		client_len = sizeof(client_addr);

		newSocket = accept(socketId, (struct sockaddr*)&client_addr,(socklen_t*)&client_len);	// Accepts connection
		if(newSocket < 0)
		{
			fprintf(stderr, "Error in Accepting connection !\n");
			exit(1);
		}


		// Getting IP address and port number of client

		struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
		struct in_addr ip_addr = client_pt->sin_addr;
		char str[INET_ADDRSTRLEN];										// INET_ADDRSTRLEN: Default ip address size
		inet_ntop( AF_INET, &ip_addr, str, INET_ADDRSTRLEN );
		printf("New Client connected with port no.: %d and ip address: %s \n",ntohs(client_addr.sin_port), str);


		//------------------------------------------------------------------------------------------------
		// Forks new client

		i = findAvailableChild(i);

		if(i>= 0 && i < MAX_CLIENTS)
		{
			ret = fork();

			if(ret == 0)									// Create child process
			{
				respondClient(newSocket);
				//printf("######################################\nChild %d closed\n######################################\n", i);
				exit(0);									// Child exits
			}
			else
			{
				printf("--------------------------------------\nChild %d Created with PID = %d\n------------------------------------\n", i,ret);
				client_PID[i] = ret;

			}
		}
		else
		{
			i = 0;
			close(newSocket);
			printf("No more Client can connect!\n");
		}


		// And goes back to listen again for another client
	}

	close(socketId);									// Close socket

 	return 0;
}
