#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>

extern char *optarg;
extern int optind, opterr, optopt;

char* const DEFAULT_HOSTNAME = "localhost";
char* const DEFAULT_SERVICE = "2020";

int main (int argc, char** argv) {

	char* hostname;
	char* service;
	int so_oobinline = 0;
	int use_poll = 0;
	int check_writable = 0;
	float timeout = 1.0;
	int timeout_ms = 1000;
	struct timeval timeout_tv = {1, 0};
	size_t buffer_size = 1024;
	
	int opt;

	while ((opt = getopt(argc, argv, "swPb:t:")) != -1) {
		switch (opt) {
			case 's':
			so_oobinline = 1;
			break;
			
			case 'P':
			use_poll = 1;
			break;
			
			case 'w':
			check_writable = 1;
			break;
			
			case 'b':
			buffer_size = strtol(optarg, NULL, 0);
			break;

			case 't':
			timeout = strtof(optarg, NULL);
			if (timeout < 0.0 || timeout > 10.0) {
				fprintf(stderr, "Specified timeout must be 0.0 < t < 10.0\n");
				exit(EXIT_FAILURE);
			}
			timeout_tv.tv_sec = timeout;
			timeout_tv.tv_usec = (timeout - timeout_tv.tv_sec) * 1e6;
			timeout_ms = (timeout_tv.tv_sec * 1000) + (timeout_tv.tv_usec / 1000);
			break;


	   default: /* '?' */
		   fprintf(stderr, "Usage: %s [-s] [-w] [-P] [-b size] [-t seconds] hostname port\n", argv[0]);
   		   fprintf(stderr, "          -P use poll() instead of select()\n");
		   fprintf(stderr, "          -s set SO_OOBINLINE\n");
		   fprintf(stderr, "          -w express interest in writability from select/poll\n");
		   exit(EXIT_FAILURE);
	   }
	}

	if (argc>optind) hostname = strdup(argv[optind]);
	else hostname = strdup(DEFAULT_HOSTNAME);
	if (argc>optind+1) service = strdup(argv[optind+1]);
	else service = strdup(DEFAULT_SERVICE);	

    struct addrinfo hints = {0};
    struct addrinfo* rp;
    struct addrinfo* result;

    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    int rc = getaddrinfo(hostname, service, &hints, &result);
    if (rc != 0) {
       fprintf(stderr, "getaddrinfo(\"%s\"): %s\n", hostname, gai_strerror(rc));
       exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "Resolving %s: ", hostname);
    
    int sfd = -1;
    for (rp=result; rp != NULL; rp=rp->ai_next) {
        char straddr[INET6_ADDRSTRLEN];
        
        if (inet_ntop(
                rp->ai_family,
                &((struct sockaddr_in*)(rp->ai_addr))->sin_addr,
                straddr,
                sizeof(straddr)
            )!=NULL) {
            fprintf(stderr, "trying %s... ", straddr);
        } else {
            perror("inet_ntop()");
            continue;
        }

        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            perror("socket");
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen)==-1) {
            perror("connect()");
            close(sfd); sfd = -1;
            continue;
        }
        
        fprintf(stderr, "OK\n");
        break;
	}

    freeaddrinfo(result);
    if (sfd==-1) {
        fprintf(stderr, "unable to connect\n");
        exit(EXIT_FAILURE);
    }

	if (setsockopt(sfd, SOL_SOCKET, SO_OOBINLINE,
                      &so_oobinline, sizeof(so_oobinline))==-1) {
		perror("setsockopt SO_OOBINLINE");
	}


    char* buffer = malloc(buffer_size);
    ssize_t bytes;
    size_t total = 0;
    fd_set rfd, wfd, efd;
	struct pollfd p = { sfd, POLLIN|POLLPRI|POLLERR|POLLHUP, 0 };

    // fprintf(stderr, "timeval t = { %ld, %ld }, timeout ms = %d\n", timeout_tv.tv_sec, (long) timeout_tv.tv_usec, timeout_ms);
	
	if (check_writable) p.events |= POLLOUT;

	if (use_poll) {

		while (1) {
			
			rc = poll(&p, 1, timeout_ms);
			
			if (rc==-1) {
				perror("poll");
				if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
				else break;
			}
			if (rc==1) {
				
				fprintf(stderr, "FD %d ready: %s%s%s%s%s\n", sfd,
					p.revents & POLLIN ? "POLLIN ":"",
					p.revents & POLLPRI ? "POLLPRI ":"",
					p.revents & POLLOUT ? "POLLOUT ":"",
					p.revents & POLLERR ? "POLLERR ":"",
					p.revents & POLLHUP ? "POLLHUP ":"");

				if (p.revents & POLLIN || p.revents & POLLPRI) {
					bytes = read(sfd, buffer, buffer_size);
					if (bytes==-1) {
						perror("read");
						if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
						else break;
					}
					if (bytes>0) {
						fprintf(stderr, "FD %d: Read %ld byte(s)\n", sfd, bytes);
						total += bytes;                
					}
					if (bytes==0) {
						fprintf(stderr, "FD %d: EOF\n", sfd);
						break; /* job's a good 'un */
					}
				} 
			}

			if (rc==0) fprintf(stderr, "Waiting\n");
		}
		
	} else {

		while (1) {

			struct timeval t = timeout_tv;
			
			FD_ZERO(&rfd);
			FD_ZERO(&wfd);
			FD_ZERO(&efd);
			
			FD_SET(sfd, &rfd);
			if (check_writable) FD_SET(sfd, &wfd);
			FD_SET(sfd, &efd);
			
			rc = select(sfd + 1, &rfd, &wfd, &efd, &t);
			
			if (rc==-1) {
				perror("select");
				if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
				else break;
			}
			
			if (rc>0) {
				
				fprintf(stderr, "FD %d ready: %s%s%s\n", sfd,
					FD_ISSET(sfd, &rfd)? "readable ":"",
					FD_ISSET(sfd, &wfd) ? "writable ":"",
					FD_ISSET(sfd, &efd) ? "exceptional ":"");
				
				if (FD_ISSET(sfd, &rfd) || FD_ISSET(sfd, &efd)) {
					
					bytes = read(sfd, buffer, buffer_size);
					if (bytes==-1) {
						perror("read");
						if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
						else break;
					}
					
					if (bytes>0) {
						fprintf(stderr, "FD %d: Read %ld byte(s)\n", sfd, bytes);
						total += bytes;                
					}
					if (bytes==0) {
						fprintf(stderr, "FD %d: EOF\n", sfd);
						break; /* job's a good 'un */
					}
				}
			}
			
			if (rc==0) fprintf(stderr, "Waiting\n");
			
		}		
	}
    
    fprintf(stderr, "Read %lu total byte(s)\n", total);
    close(sfd);
}
