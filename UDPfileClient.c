#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <readline/readline.h>


#define PORT "40071"

#define BLOCKSIZE 1024
#define TMPFILE "filecli.XXXXXX"


void usage(const char* progname)
{
    fprintf(stderr, "usage: %s <hostname>\n", progname);
    exit(EXIT_FAILURE);
}


void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


struct udp_data
{
    int blkno;
    char data[BLOCKSIZE];
};

struct udp_data_list
{
    struct udp_data filedata;
    struct udp_data_list* next;
};

struct udp_data* blksearch(struct udp_data_list* blklist, int blkno)
{
    int found = 0;

    while (blklist)
    {
        if (blklist->filedata.blkno == blkno)
        {
            found = 1;
            break;
        }

        blklist = blklist->next;
    }

    if (found)
        return &blklist->filedata;

    return NULL;
}

void blkwrite(int nblocks, int lbsize, struct udp_data_list* blklist)
{
    char template[] = TMPFILE;
    int fd = mkstemp(template);

    int i;
    struct udp_data* pdata;
    for (i = 0; i < nblocks; i++)
    {
        if (pdata = blksearch(blklist, i))
            write(fd, pdata->data, (i == nblocks-1) ? lbsize : BLOCKSIZE);
    }

    close(fd);
}

void blkfree(struct udp_data_list* blklist)
{
    struct udp_data_list* next;
    while (blklist)
    {
        next = blklist->next;
        free(blklist);
        blklist = next;
    }
}


int main(int argc, char* argv[])
{
    if (argc < 2)
        usage(argv[0]);

    struct addrinfo hints;
    explicit_bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc;
    struct addrinfo* srvinfo;
    if (rc = getaddrinfo(argv[1], PORT, &hints, &srvinfo))
    {
        fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    struct addrinfo* p;
    int sockfd;
    for (p = srvinfo; p; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("error: socket");
            continue;
        }

        break;
    }

    if (!p)
    {
        close(sockfd);
        freeaddrinfo(srvinfo);
        srvinfo = NULL;

        fprintf(stderr, "error: cannot find ip\n");
        return EXIT_FAILURE;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(
        p->ai_family,
        get_in_addr((struct sockaddr*)p->ai_addr),
        s, sizeof(s)
    );

    printf("%s will be used as a server\n", s);

    rc = EXIT_SUCCESS;

    int running = 1;
    char* msg;
    int nbytes;
    uint8_t status;
    uint32_t filesize;
    int nblocks, lbsize;
    int i;
    struct udp_data_list *blklist, *pblock;
    while (running)
    {
        msg = readline("enter filename to be requested: ");

        if (!msg || (strcmp(msg, "") == 0))
        {
            free(msg);
            break;
        }

        if ((nbytes = sendto(
                sockfd,
                msg, strlen(msg)+1,
                0,
                p->ai_addr, p->ai_addrlen)) == -1)
        {
            free(msg);
            rc = EXIT_FAILURE;

            perror("error: sendto");
            break;
        }

        if ((nbytes = recvfrom(
                sockfd,
                &status, sizeof(status),
                0,
                p->ai_addr, &p->ai_addrlen)) <= 0)
        {
            free(msg);
            rc = EXIT_FAILURE;

            perror("error: recvfrom");
            break;
        }

        if (status)
        {
            free(msg);

            puts("the requested file does not exist");
            continue;
        }

        if ((nbytes = recvfrom(
                sockfd,
                &filesize, sizeof(filesize),
                0,
                p->ai_addr, &p->ai_addrlen)) <= 0)
        {
            free(msg);
            rc = EXIT_FAILURE;

            perror("error: recvfrom");
            break;
        }
        filesize = ntohl(filesize);

        printf("file size is %d bytes", filesize);

        nblocks = (filesize / BLOCKSIZE) + 1;
        lbsize = filesize % BLOCKSIZE;

        printf(
            (nblocks == 1)
                ? ", %d block will be used\n"
                : ", %d blocks will be used\n",
            nblocks
        );

        puts("downloading file...");

        blklist = malloc(sizeof(struct udp_data_list));
        pblock = blklist;

        for (i = 0; i < nblocks; i++)
        {
            if ((nbytes = recvfrom(
                    sockfd,
                    &pblock->filedata, sizeof(struct udp_data),
                    0,
                    p->ai_addr, &p->ai_addrlen)) <= 0)
            {
                running = 0;
                rc = EXIT_FAILURE;

                perror("error: recvfrom");
                break;
            }

            printf("received block %d\n", pblock->filedata.blkno);

            if (i == nblocks-1)
            {
                pblock->next = NULL;
                break;
            }

            pblock->next = malloc(sizeof(struct udp_data_list));
            pblock = pblock->next;
        }

        blkwrite(nblocks, lbsize, blklist);

        free(msg);
        blkfree(blklist);
    }

    msg = NULL;
    blklist = NULL;

    close(sockfd);
    freeaddrinfo(srvinfo);
    srvinfo = NULL;

    return rc;
}
