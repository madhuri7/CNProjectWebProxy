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

char *url_blacklist[] = { "www.facebook.com", "www.youtube.com", "www.hulu.com","www.virus.com" };
int url_blacklist_len = 4;

char *word_blacklist[] = {"Alcoholic","Amateur","Analphabet","Anarchist","Ape","Arse","Arselicker","Ass","Ass master","Ass-kisser","Ass-nugget","Ass-wipe","Asshole","Baby","Backwoodsman","Balls","Bandit","Barbar","Bastard","Bastard","Beavis","Beginner","Biest","Bitch","Blubber gut","Bogeyman","Booby","Boozer","Bozo","Brain-fart","Brainless","Brainy","Brontosaurus","Brownie","Bugger","Bugger", "silly","Bulloks","Bum","Bum-fucker","Butt","Buttfucker","Butthead","Callboy","Callgirl","Camel","Cannibal","Cave man","Chaavanist","Chaot","Chauvi","Cheater","Chicken","Children fucker","Clit","Clown","Cock","Cock master","Cock up","Cockboy","Cockfucker","Cockroach","Coky","Con merchant","Con-man","Country bumpkin","Cow","Creep","Creep","Cretin","Criminal","Cunt","Cunt sucker","Daywalker","Deathlord","Derr brain","Desperado","Devil","Dickhead","Dinosaur","Disguesting packet","Diz brain","Do-Do","Dog","Dog, dirty","Dogshit","Donkey","Drakula","Dreamer","Drinker","Drunkard","Dufus","Dulles","Dumbo","Dummy","Dumpy","Egoist","Eunuch","Exhibitionist","Fake","Fanny","Farmer","Fart","Fart, shitty","Fatso","Fellow","Fibber","Fish","Fixer","Flake","Flash Harry","Freak","Frog","Fuck","Fuck face","Fuck head","Fuck noggin","Fucker","Head, fat","Hell dog","Hillbilly","Hooligan","Horse fucker","Idiot","Ignoramus","Jack-ass","Jerk","Joker","Junkey","Killer","Lard face","Latchkey child","Learner","Liar","Looser","Lucky","Lumpy","Luzifer","Macho","Macker","Man, old","Minx","Missing link","Monkey","Monster","Motherfucker","Mucky pub","Mutant","Neanderthal","Nerfhearder","Nobody","Nurd","Nuts, numb","Oddball","Oger","Oil dick","Old fart","Orang-Uthan","Original","Outlaw","Pack","Pain in the ass","Pavian","Pencil dick","Pervert","Pig","Piggy-wiggy","Pirate","Pornofreak","Prick","Prolet","Queer","Querulant","Rat","Rat-fink","Reject","Retard","Riff-Raff","Ripper","Roboter","Rowdy","Rufian","Sack","Sadist","Saprophyt","Satan","Scarab","Schfincter","Shark","Shit eater","Shithead","Simulant","Skunk","Skuz bag","Slave","Sleeze","Sleeze bag","Slimer","Slimy bastard","Small pricked","Snail","Snake","Snob","Snot","Son of a bitch ","Square","Stinker","Stripper","Stunk","Swindler","Swine","Teletubby","Thief","Toilett cleaner","Tussi","Typ","Unlike","Vampir","Vandale","Varmit","Wallflower","Wanker","Wanker, bloody","Weeze Bag","Whore","Wierdo","Wino","Witch","Womanizer","Woody allen","Worm","Xena","Xenophebe","Xenophobe","XXX Watcher","Yak","Yeti","Zit face"};
int word_blacklist_len = 237;


int main(int argc, char **argv)
{
	
    //Declaration of sockets and size variables
    pid_t proc_id;
	struct sockaddr_in addr_in, cli_addr, serv_addr;
	struct hostent *hostent;
	int socket_id, newsocket_id;
	int clilen = sizeof(cli_addr);
	struct stat st = {0};
	
	if(argc != 2)
	{
		//checking for arguments
        printf("Using the following port:\n\t%s <port>\n", argv[0]);
		return -1;
	}
  
	printf("starting the proxy...\n");
	
    //Creating a directory for cache
	if (stat("./CacheDir/", &st) == -1) {
		mkdir("./CacheDir/", 0700);
	}
   
	bzero((char*)&serv_addr, sizeof(serv_addr));
	bzero((char*)&cli_addr, sizeof(cli_addr));
	   
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr = INADDR_ANY;
   
	//Initializing sockets and binding
    socket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(socket_id < 0)
	{
		perror("failed to initialize given socket");
	}
   
	if(bind(socket_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("failed to bind socket to process");
	}

	listen(socket_id, 50); // listen for requests
	
accepting:
	newsocket_id = accept(socket_id, (struct sockaddr*)&cli_addr, &clilen); // accepting requests
	
    
    //multithreading
	if((proc_id = fork()) == 0)
	{
		struct sockaddr_in host_addr;
		int i, n;
		int rsocket_id;
		int cfd;
		int port = 80;
		char type[256];
		char url[4096];
		char proto[256];
		
		char datetime[256];
		
		
		char url_host[256], url_path[256];
		
		char url_encoded[4096];
        char filepath[256];
		
		char *dateptr;
		char buffer[4096];
		int response_code;
		
		bzero((char*)buffer, 4096);
		
		
		recv(newsocket_id, buffer, 4096, 0);
		
		
		sscanf(buffer, "%s %s %s", type, url, proto);
		
		
		if(url[0] == '/')
		{
			strcpy(buffer, &url[1]);
			strcpy(url, buffer);
		}
		
		
		//Checking for validity of requested url
		if((strncmp(type , "GET", 3) != 0) || ((strncmp(proto, "HTTP/1.1", 8) != 0) && (strncmp(proto, "HTTP/1.0", 8) != 0)))
		{
			
			sprintf(buffer,"ERROR 400 : NOT A GET REQUEST");
			send(newsocket_id, buffer, strlen(buffer), 0);
			
			
			goto end;
		}
		
		parse_url(url, url_host, &port, url_path);
		
		url_encode(url, url_encoded);

        //Filtering blacklisted URLs
		for(i = 0; i < url_blacklist_len; i++)
		{
			if(NULL != strstr(url, url_blacklist[i]))
			{
				sprintf(buffer,"ERROR : CANNOT ACCESS BLACKLISTED URL \n%s", url_blacklist[i]);
				send(newsocket_id, buffer, strlen(buffer), 0);
				
				goto end;
			}
		}
		
		if((hostent = gethostbyname(url_host)) == NULL)
		{
			fprintf(stderr, "failed to resolve host %s: %s\n", url_host, strerror(errno));
			goto end;
		}
		
		bzero((char*)&host_addr, sizeof(host_addr));
		host_addr.sin_port = htons(port);
		host_addr.sin_family = AF_INET;
		bcopy((char*)hostent->h_addr, (char*)&host_addr.sin_addr.s_addr, hostent->h_length);

		rsocket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		
		if(rsocket_id < 0)
		{
			perror("failed to create remote socket");
			goto end;
		}
				
		if(connect(rsocket_id, (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0)
		{
			perror("failed to connect to remote server");
			goto end;
		}

        //Adding to cache
		sprintf(filepath, "./CacheDir/%s", url_encoded);
		if (0 != access(filepath, 0)) {
			sprintf(buffer,"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);
			goto request;
		}
		
		sprintf(filepath, "./CacheDir/%s", url_encoded);
		cfd = open (filepath, O_RDWR);
		bzero((char*)buffer, 4096);
		read(cfd, buffer, 4096);
		close(cfd);
		
		dateptr = strstr(buffer, "Date:");
		if(NULL != dateptr)
		{
			
			bzero((char*)datetime, 256);
			strncpy(datetime, &dateptr[6], 29);
			
			sprintf(buffer,"GET %s HTTP/1.0\r\nHost: %s\r\nIf-Modified-Since: %s\r\nConnection: close\r\n\r\n", url_path, url_host, datetime);
		} else {
			sprintf(buffer,"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);
		}

request:
		n = send(rsocket_id, buffer, strlen(buffer), 0);
		
		if(n < 0)
		{
			perror("failed to write to remote socket");
			goto end;
		}

do_CacheDir: //updating cache files
		cfd = -1;
		
		do
		{
			bzero((char*)buffer, 4096);
			
			n = recv(rsocket_id, buffer, 4096, 0);
			if(n > 0)
			{
				if(cfd == -1)
				{
					float ver;
					sscanf(buffer, "HTTP/%f %d", &ver, &response_code);
					
					if(response_code != 304)
					{
						sprintf(filepath, "./CacheDir/%s", url_encoded);
						if((cfd = open(filepath, O_RDWR|O_TRUNC|O_CREAT, S_IRWXU)) < 0)
						{
							perror("failed to create CacheDir file");
							goto end;
						}
					} else {
						goto from_CacheDir;
					}
				}
                
                //Filtering blacklisted content
				for(i = 0; i < word_blacklist_len; i++)
				{
					if(NULL != strstr(buffer, word_blacklist[i]))
					{
						
						close(cfd);
						
						sprintf(filepath, "./CacheDir/%s", url_encoded);
						remove(filepath);
						
						sprintf(buffer,"ERROR !!! CONTENT HAS BLACKLISTED WORD\n%s", word_blacklist[i]);
						send(newsocket_id, buffer, strlen(buffer), 0);
						goto end;
					}
				}
				
				write(cfd, buffer, n);
			}
		} while(n > 0);
		close(cfd);
		

from_CacheDir: //reading from cache files
		
		sprintf(filepath, "./CacheDir/%s", url_encoded);
		if((cfd = open (filepath, O_RDONLY)) < 0)
		{
			perror("failed to open CacheDir file");
			goto end;
		}
		do
		{
			bzero((char*)buffer, 4096);
			n = read(cfd, buffer, 4096);
			if(n > 0)
			{
				send(newsocket_id, buffer, n, 0);
			}
		} while(n > 0);
		close(cfd);

end:
		close(rsocket_id);
		close(newsocket_id);
		close(socket_id);
		return 0;
	} else {
		close(newsocket_id);
        goto accepting;
	}
	
	close(socket_id);
	return 0;
}
