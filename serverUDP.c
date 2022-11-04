#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <readline/readline.h>


#define PORT "40071"


void sigchld_handler(int s)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) ;
}


void client_disconnects(int signum)
{
    puts("broken pipe"); 
}


void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//rc - return code
//variable/pointer which can change from different stream or process 
//we use it to see 
struct reply_loop_args
{
    int* rc_ptr;
    int* running_ptr;
    int sockfd;
    volatile struct sockaddr_storage* other_ptr;
    socklen_t other_len;
};

void* reply_loop(void* a)
{
    struct reply_loop_args* args = a;

    volatile struct sockaddr_storage saved_other = *(args->other_ptr);

    char* msg;
    int nbytes;
    while (1)
    {
	    //saved_other - if *(rgs->other_ptr) will change so here we saved it in beginning 
        if ( (((struct sockaddr_in*)args->other_ptr)->sin_addr.s_addr
                != ((struct sockaddr_in*)&saved_other)->sin_addr.s_addr)
             && (((struct sockaddr_in6*)args->other_ptr)->sin6_addr.s6_addr
                != ((struct sockaddr_in6*)&saved_other)->sin6_addr.s6_addr) )
        {
            free(msg);
            break;
        }

        msg = readline(NULL);

        if (!msg || (strcmp(msg, "") == 0))
        {
            free(msg);
            msg = NULL;
	//running_ptr - we change value of running in main
            *(args->running_ptr) = 0;
            break;
        }

        if ((nbytes = sendto(
                args->sockfd,
                msg, strlen(msg)+1,
                0,
                (struct sockaddr*)args->other_ptr, args->other_len) == -1))
        {
            free(msg);
            msg = NULL;
            *(args->rc_ptr) = EXIT_FAILURE;

            perror("error: sendto");

            *(args->running_ptr) = 0;
            break;
        }

        free(msg);
    }

    msg = NULL;

    return NULL;
}


int main(int argc, char* argv[])
{
    struct addrinfo hints;
    explicit_bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int rc;
    struct addrinfo* srvinfo;
    if (rc = getaddrinfo(NULL, PORT, &hints, &srvinfo))
    {
        fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    struct addrinfo* p;
    int sockfd;
    
    struct timeval rd_timeout;
    for (p = srvinfo; p; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("error: socket");
            continue;
        }
	//means time interval 
        rd_timeout.tv_sec = 0;
        rd_timeout.tv_usec = 10;
	//receive timout
	//setsockopt put func SO_RCVTIMEO - responsible for max time until our function will receive error. For expmale we can put 10 min and it will wait 10min
	//and if nothing will come it will return error(EAGAIN)
	//When receive in non-block format and out didn't receive any datagram
	
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &rd_timeout, sizeof(rd_timeout));
	
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("error: bind");
            continue;
        }

        break;
    }

    if (!p)
    {
        freeaddrinfo(srvinfo);
        srvinfo = NULL;

        fprintf(stderr, "error: failed to bind\n");
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        freeaddrinfo(srvinfo);
        srvinfo = NULL;

        perror("error: sigaction");
        return EXIT_FAILURE;
    }
	//when pipe is broken when client is disconnected we call this function
    signal(SIGPIPE, client_disconnects);

    puts("server is ready");

    rc = EXIT_SUCCESS;

    int running = 1;
    int nbytes;
    char incoming[80];
    //other - we can answer only to one client and this shows to which client we are answering 
    //prev_other - if we recceive different message from same IP
    struct sockaddr_storage other, prev_other = { 0 };
    socklen_t other_len = sizeof(other);
    char s[INET6_ADDRSTRLEN];

    struct reply_loop_args args;
    args.rc_ptr = &rc;
    args.running_ptr = &running;
    args.sockfd = sockfd;
    args.other_ptr = &other;
    args.other_len = other_len;
    //main loop for our server
    while (running)
    { 
	//recvfrom is block function 
	//until this function is working we can forever until 
	//we will get something 
	//to understand if its block or not block func you should see first arg
        if ((nbytes = recvfrom(
                sockfd,
                incoming, sizeof(incoming)-1,
                0,
                (struct sockaddr*)&other, &other_len)) == -1)
        {
		//and here we check if error is EAGAIN we rerun it
            if (errno == EAGAIN)
                continue;

            running = 0;
            rc = EXIT_FAILURE;

            perror("error: recvfrom");
            break;
        }

        incoming[nbytes] = 0;

        inet_ntop(
            other.ss_family,
            get_in_addr((struct sockaddr*)&other),
            s, sizeof(s)
        );

        printf("\nmessage from %s: %s\n", s, incoming);

        pthread_t thread;
	
        if ( (((struct sockaddr_in*)&other)->sin_addr.s_addr
                != ((struct sockaddr_in*)&prev_other)->sin_addr.s_addr)
             && (((struct sockaddr_in6*)&other)->sin6_addr.s6_addr
                != ((struct sockaddr_in6*)&prev_other)->sin6_addr.s6_addr) )
        {
		//reply_loop - send answer to client 
            pthread_create(&thread, 0, reply_loop, &args);
            prev_other = other;
        }
    }

    close(sockfd);
    freeaddrinfo(srvinfo);
    srvinfo = NULL;

    return rc;
}
