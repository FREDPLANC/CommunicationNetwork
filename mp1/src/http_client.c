#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 500 

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
	int sockfd, numbytes;  
	static char buf[1000];
	char recvingbuf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

    int size=strlen(argv[1]);

    char *url = argv[1];

    // Define variables to store the extracted components
    char hostname[256] = "";
    char port[6] = "80";  // Default port is 80
    char path[256] = "/"; // Default path is "/"

    // Check if the URL starts with "http://"
    if (strncmp(url, "http://", 7) == 0) {
        char *temp = url + 7; // Move the pointer past "http://"

        // Extract the hostname
        char *slash_pos = strchr(temp, '/');
        if (slash_pos) {
            strncpy(hostname, temp, slash_pos - temp);
            hostname[slash_pos - temp] = '\0';
            temp = slash_pos; // Move the pointer to the path part
        } else {
            strcpy(hostname, temp);
            temp = ""; // No path
        }

        // Extract the port, if specified
        char *colon_pos = strchr(hostname, ':');
        if (colon_pos) {
            strcpy(port, colon_pos + 1);
            *colon_pos = '\0'; // Null-terminate the hostname at the colon
        }

        // Extract the path
        strcpy(path, temp);
    } else {
        fprintf(stderr, "Invalid URL format. URL should start with 'http://'\n");
        return 1;
    }

    printf("Hostname: %s\n", hostname);
    printf("Port: %s\n", port);
    printf("Path: %s\n", path);

		
	//@@set the format of http information
	sprintf(buf,"GET ");
	strcat(buf,path);
	strcat(buf," HTTP/1.1\r\n");
	strcat(buf,"User-Agent: Wget/1.12 (linux-gnu)\r\n");
	strcat(buf,"Host: ");
	strcat(buf,hostname);
	strcat(buf,":");
	strcat(buf,port);
	strcat(buf,"\r\n");
	strcat(buf,"Connection: Keep-Alive\r\n\r\n");
	
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port[0] ? port : "80", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    send(sockfd, buf, strlen(buf), 0);
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    freeaddrinfo(servinfo);

    FILE *fp;
    fp = fopen("output", "w");
    while(1) {
        memset(recvingbuf, 0, sizeof(recvingbuf));
        if((numbytes = recv(sockfd, recvingbuf, MAXDATASIZE-1, 0)) > 0) {
            fwrite(recvingbuf, 1, numbytes, fp);
            printf("num in line: %d\n", numbytes);
        } else {
            fclose(fp);
            break;
        }
    }
    close(sockfd);
    return 0;
}
