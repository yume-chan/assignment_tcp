#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

/* Subtract the ‘struct timeval’ values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec)
	{
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000)
	{
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
     tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

int main(int argc, char *argv[])
{
	int sockfd;

	struct sockaddr_in server_address;
	int rv;
	int numbytes, numread;
	char buffer[1450];

	struct timeval GTOD_before, GTOD_after, difference;

	char **tokens;
	FILE *fptr;

	if (argc != 3)
	{
		fprintf(stderr, "usage: %s <HOSTNAME:PORT> <FILENAME>\n", argv[0]);
		exit(1);
	}

	char delim[] = ":";
	char *Desthost = strtok(argv[1], delim);
	char *Destport = strtok(NULL, delim);
	char *filename = argv[2];

	printf("Opening %s sending to %s:%s.\n", filename, Desthost, Destport);

	FILE *file = fopen(filename, "r");
	if (file == NULL)
	{
		printf("open file failed.\n");
		exit(-1);
	}

	fseek(file, 0, SEEK_END);
	numbytes = ftell(file);
	fseek(file, 0, SEEK_SET);

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, Desthost, &server_address.sin_addr);
	server_address.sin_port = htons(strtol(Destport, NULL, 10));
	if (connect(sockfd, &server_address, sizeof(server_address)) < 0)
	{
		printf("connect failed.\n");
		exit(-1);
	}

	printf("Connected, Sending");

	gettimeofday(&GTOD_before, NULL);

	while (numread < numbytes)
	{
		int read = fread(buffer, 1, sizeof(buffer), file);
		send(sockfd, buffer, read, 0);
		numread += read;
	}

	gettimeofday(&GTOD_after, NULL);

	fclose(file);
	close(sockfd);

	timeval_subtract(&difference, &GTOD_after, &GTOD_before);

	printf(", Sent %i bytes, ", numbytes);
	printf("in %ld us.\n", difference.tv_sec * 1000000 + difference.tv_usec);

	return 0;
}
