#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "proxy.h"

char *blacklisted_urls[] = { "www.facebook.com", "www.youtube.com", "www.hulu.com","www.virus.com" };
int blacklisted_urls_len = 3;

char *blacklisted_words[] = {"fake", "fixer", "algorithms"};
int blacklisted_words_len = 3;

int main(int argc, char **argv)
{
	pid_t chpid;
	struct sockaddr_in addr_in, cli_addr, serv_addr;
	struct hostent *hostent;
	int sockfd, newsockfd;
	int clilen = sizeof(cli_addr);
	struct stat st = {0};
	
	// takes port number as argument
	if(argc != 2)
	{
		printf("Using:\n\t%s <port>\n", argv[0]);
		return -1;
	}
  
	printf("Proxy Server is starting...\n");
	
	// checking if the cache directory exists
	if (stat("./URLCache/", &st) == -1) {
		mkdir("./URLCache/", 0700);
	}
   
	bzero((char*)&serv_addr, sizeof(serv_addr));
	bzero((char*)&cli_addr, sizeof(cli_addr));
	   
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr = INADDR_ANY;
   
	// creating the listening socket for our proxy server
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0)
	{
		perror("failed to initialize socket");
	}
   
	// binding our socket to the given port
	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("failed to bind to socket");
	}

	listen(sockfd, 50);
	
accepting:
	newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
	
	if((chpid = fork()) == 0)
	{
		struct sockaddr_in host_addr;
		int i, n;			// loop indices
		int rsockfd;		// remote socket file descriptor
		int cfd;			// cache-file file descriptor
		
		int port = 80;		// default http port - can be overridden	
		char type[256];		// type of request - e.g. GET/POST/HEAD
		char url[4096];		// url in request - e.g. facebook.com
		char proto[256];	// protocol ver in request - e.g. HTTP/1.0
		
		char datetime[256];	// the date-time when we last cached a url
		
		char host_url[256], path_url[256];
		
		char url_encoded[4096];	// encoded url, used for cahce filenames
		char filepath[256]; 	// used for cache file paths
		
		char *dateptr;		// used to find the date-time in http response
		char buffer[4096];	// buffer used for send/receive
		int response_code;	// http response code - e.g. 200, 304, 301
		
		bzero((char*)buffer, 4096);			// let's play it safe!
		
		recv(newsockfd, buffer, 4096, 0);
		
		sscanf(buffer, "%s %s %s", type, url, proto);
		
		if(url[0] == '/')
		{
			strcpy(buffer, &url[1]);
			strcpy(url, buffer);
		}
		
		printf("-> %s %s %s\n", type, url, proto);
		
		if((strncmp(type , "GET", 3) != 0) || ((strncmp(proto, "HTTP/1.1", 8) != 0) && (strncmp(proto, "HTTP/1.0", 8) != 0)))
		{
			printf("\t-> bad request format - we only accept HTTP GETs\n");
			sprintf(buffer,"400 : BAD REQUEST\nONLY GET REQUESTS ARE ALLOWED");
			send(newsockfd, buffer, strlen(buffer), 0);
			goto end;
		}
		
		parse_url(url, host_url, &port, path_url);
		
		url_encode(url, url_encoded);
		
		printf("\t-> host_url: %s\n", host_url);
		printf("\t-> port: %d\n", port);
		printf("\t-> path_url: %s\n", path_url);
		printf("\t-> url_encoded: %s\n", url_encoded);

		// BLACK LIST CHECK
		for(i = 0; i < blacklisted_urls_len; i++)
		{
			// if url contains the black-listed word
			if(NULL != strstr(url, blacklisted_urls[i]))
			{
				printf("\t-> url in blacklist: %s\n", blacklisted_urls[i]);
				sprintf(buffer,"400 : BAD REQUEST\nURL FOUND IN BLACKLIST\n%s", blacklisted_urls[i]);
				send(newsockfd, buffer, strlen(buffer), 0);
				
				goto end;
			}
		}
		
		if((hostent = gethostbyname(host_url)) == NULL)
		{
			fprintf(stderr, "failed to resolve %s: %s\n", host_url, strerror(errno));
			goto end;
		}
		
		bzero((char*)&host_addr, sizeof(host_addr));
		host_addr.sin_port = htons(port);
		host_addr.sin_family = AF_INET;
		bcopy((char*)hostent->h_addr, (char*)&host_addr.sin_addr.s_addr, hostent->h_length);

		rsockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		
		if(rsockfd < 0)
		{
			perror("failed to create remote socket");
			goto end;
		}
				
		if(connect(rsockfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0)
		{
			perror("failed to connect to remote server");
			goto end;
		}

		printf("\t-> connected to host: %s w/ ip: %s\n", host_url, inet_ntoa(host_addr.sin_addr));
				
			goto request; 
		}
		
		
		dateptr = strstr(buffer, "Date:");
		if(NULL != dateptr)
		{
			
			bzero((char*)datetime, 256);
			strncpy(datetime, &dateptr[6], 29);
			
			sprintf(buffer,"GET %s HTTP/1.0\r\nHost: %s\r\nIf-Modified-Since: %s\r\nConnection: close\r\n\r\n", path_url, host_url, datetime);
			printf("\t-> conditional GET...\n");
			printf("\t-> If-Modified-Since: %s\n", datetime);
		} else {
			sprintf(buffer,"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path_url, host_url);
			printf("\t-> the response had no date field\n");
		}

request:
		n = send(rsockfd, buffer, strlen(buffer), 0);
		
		if(n < 0)
		{
			perror("failed to write to remote socket");
			goto end;
		}

do_cache:
		cfd = -1;
		
		do
		{
			bzero((char*)buffer, 4096);
			
			n = recv(rsockfd, buffer, 4096, 0);
			if(n > 0)
			{
				if(cfd == -1)
				{
					float ver;
					sscanf(buffer, "HTTP/%f %d", &ver, &response_code);
					
					printf("\t-> response_code: %d\n", response_code);
					}
				
				for(i = 0; i < blacklisted_words_len; i++)
				{
					if(NULL != strstr(buffer, blacklisted_words[i]))
					{
						printf("\t-> content in blacklist: %s\n", blacklisted_words[i]);
						
						close(cfd);
						
						sprintf(filepath, "./cache/%s", url_encoded);
						remove(filepath);
						
						sprintf(buffer,"400 : BAD REQUEST\nCONTENT FOUND IN BLACKLIST\n%s", blacklisted_words[i]);
						send(newsockfd, buffer, strlen(buffer), 0);			goto end;
					}
				}
				
			}
		} while(n > 0);
	
end:
		printf("\t-> exiting...\n");
		close(rsockfd);
		close(newsockfd);
		close(sockfd);
		return 0;
	} else {
		close(newsockfd);
		
		goto accepting;
	}
	
	close(sockfd);
	return 0;
}
