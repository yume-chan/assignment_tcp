#include <netdb.h>
#include <netinet/in.h> //INADDR_ANY
#include <stdio.h>
#include <stdlib.h>     //sizeof
#include <string.h>     //memset
#include <sys/socket.h> //socket
#include <sys/time.h>
#include <sys/types.h> //socket

#include <time.h>

typedef struct linked_list_node
{
  void *value;
  struct linked_list_node *next;
} linked_list_node_t;

void linked_list_append(linked_list_node_t *head, void *value)
{
  while (head->next != NULL)
  {
    head = head->next;
  }

  linked_list_node_t *node = malloc(sizeof(linked_list_node_t));
  node->value = value;
  node->next = NULL;

  head->next = node;
}

void linked_list_remove(linked_list_node_t **head, void *value)
{
  if ((*head)->value == value)
  {
    head = (*head)->next;
  }

  linked_list_node_t *current = *head;
  while (current->next != NULL)
  {
    linked_list_node_t *next = current->next;

    if (next->value == value)
    {
      current->next = next->next;
      free(next);
      break;
    }

    current = next;
  }
}

typedef struct client_info
{
  int fd;
  struct addrinfo *address;
  int address_len;
  int received;
  int connected_time;
} client_info_t;

/*
	Calculate the difference between to timevals; store result in an timeval.
	syntax: a,b,c.
	Result: c=a-b;
*/
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

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//Convert a struct sockaddr address to a string, IPv4 and IPv6:
char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  switch (sa->sa_family)
  {
  case AF_INET:
    inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
              s, maxlen);
    break;

  case AF_INET6:
    inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
              s, maxlen);
    break;

  default:
    strncpy(s, "Unknown AF", maxlen);
    return NULL;
  }

  return s;
}

static const char *program_name;

int main(int argc, char *argv[])
{
  int listenfd; //to create socket
  int connfd;   //to accept connection

  struct sockaddr_in serverAddress; //server receive on this address
  struct sockaddr_in clientAddress; //server sends to client on this address

  char msg[1450];
  int clientAddressLength;

  const char *separator = strrchr(argv[0], '/');
  if (separator)
  {
    program_name = separator + 1;
  }
  else
  {
    program_name = argv[0];
  }

  listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons(strtol(argv[1], NULL, 10));
  if (bind(listenfd, &serverAddress, sizeof(serverAddress)) < 0)
  {
    printf("error");
    return -1;
  }

  if (listen(listenfd, 5) < 0)
  {
    printf("error");
    return -1;
  }

  fd_set set;
  linked_list_node_t *clients = malloc(sizeof(linked_list_node_t));

  while (1)
  {
    FD_ZERO(&set);

    FD_SET(listenfd, &set);
    int nfd = listenfd;

    linked_list_node_t *current = clients;
    while (current->next != NULL)
    {
      current = current->next;

      client_info_t *info = current->value;
      int fd = info->fd;

      FD_SET(fd, &set);

      if (fd > nfd)
      {
        nfd = fd;
      }
    }

    if (select(nfd + 1, &set, NULL, NULL, NULL) < 0)
    {
      printf("error");
      return -1;
    }

    current = clients;
    linked_list_node_t *last;
    while (current->next != NULL)
    {
      last = current;
      current = current->next;

      client_info_t *info = current->value;
      int fd = info->fd;

      if (FD_ISSET(fd, &set))
      {
        while (1)
        {
          int received = recv(fd, msg, sizeof(msg), MSG_DONTWAIT);

          if (received == 0)
          {
            clientAddressLength = sizeof(clientAddress);
            getpeername(fd, &clientAddress, &clientAddressLength);

            char *hostname = "N/A";
            struct hostent *host = gethostbyaddr(&clientAddress, clientAddressLength, AF_INET);
            if (host != NULL)
            {
              hostname = host->h_name;
            }

            int duration = time(NULL) - info->connected_time;

            int speed;
            if (duration == 0)
            {
              speed = info->received;
            }
            else
            {
              speed = info->received / duration;
            }

            printf("%i | %s (%s) | %i | %i | %i s\n",
                   info->connected_time,
                   get_ip_str(&clientAddress, msg, sizeof(msg)),
                   hostname,
                   info->received,
                   speed,
                   duration);

            linked_list_remove(&clients, info);
            free(info);

            current = last;
            break;
          }

          if (received < 0)
          {
            break;
          }

          info->received += received;
        }
      }
    }

    if (FD_ISSET(listenfd, &set))
    {
      client_info_t *info = malloc(sizeof(client_info_t));
      info->fd = accept(listenfd, NULL, NULL);
      info->connected_time = time(NULL);
      info->received = 0;

      linked_list_append(clients, info);
    }
  }

  return 0;
}
