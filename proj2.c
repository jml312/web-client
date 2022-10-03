/*
 * Name:        Josh Levy
 * Case ID:     jml312
 * Filename:    proj2.c
 * Created:     9/29/22
 * Description: This program will take a hostname and local file name as input
 *              and will download the file from the host to the local machine.
 *              It will also print out information about the request.
 */

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define PORT 80

/* exit program with error message */
int errexit(char *format, char *arg)
{
  fprintf(stderr, format, arg);
  fprintf(stderr, "\n");
  exit(ERROR);
}

/* creates a socket and connects to the server
 *  with the given hostname and port number.
 */
void connectsocket(char *hostname, int *socket_descriptor)
{
  struct sockaddr_in sin;
  struct hostent *hinfo;
  struct protoent *protoinfo;

  /* lookup the hostname */
  hinfo = gethostbyname(hostname);
  if (hinfo == NULL)
  {
    errexit("ERROR: cannot find hostname %s", hostname);
  }

  /* set endpoint information */
  memset((char *)&sin, 0x0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(PORT);
  memcpy((char *)&sin.sin_addr, hinfo->h_addr, hinfo->h_length);
  if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
  {
    errexit("ERROR: cannot find protocol information for %s", PROTOCOL);
  }

  /* allocate a socket */
  *socket_descriptor = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
  if (*socket_descriptor < 0)
  {
    errexit("ERROR: cannot create socket", NULL);
  }

  /* connect the socket */
  if (connect(*socket_descriptor, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    errexit("ERROR: cannot connect socket", NULL);
  }
}

int main(int argc, char *argv[])
{
  char *hostname, *url_filename, *local_filename = "";
  char buffer[BUFLEN];
  FILE *socket_pointer;
  int iflag = 0, cflag = 0, sflag = 0, fflag = 0;
  int socket_descriptor = 0;
  int status_code;

  /* read command line arguments */
  int opt;
  while ((opt = getopt(argc, argv, ":u:o:icsf")) != -1)
  {
    switch (opt)
    {
    case 'i':
      iflag = 1;
      break;
    case 'c':
      cflag = 1;
      break;
    case 's':
      sflag = 1;
      break;
    case 'f':
      fflag = 1;
      break;
    case 'u':
      if (strncasecmp(optarg, "http://", 7) != 0)
      {
        errexit("ERROR: hostname must start with http://", NULL);
      }
      hostname = strtok(optarg + 7, "/");
      char *rest = strtok(NULL, "");
      if (rest != NULL)
      {
        url_filename = malloc(strlen(rest) + 2);
        url_filename[0] = '/';
        strcpy(url_filename + 1, rest);
      }
      else
      {
        url_filename = "/";
      }
      break;
    case 'o':
      local_filename = optarg;
      break;
    case ':':
    {
      char optopt_char[2];
      sprintf(optopt_char, "%c", optopt);
      errexit("ERROR: option -%s requires an argument", optopt_char);
      break;
    }
    case '?':
    {
      char optopt_char[2];
      sprintf(optopt_char, "%c", optopt);
      errexit("ERROR: unknown option -%s", optopt_char);
      break;
    }
    }
  }

  /* check for empty hostname or output filename */
  if (strcmp(hostname, "") == 0)
  {
    errexit("ERROR: hostname is required", NULL);
  }
  if (strcmp(local_filename, "") == 0)
  {
    errexit("ERROR: output filename is required", NULL);
  }

  do
  {
    if (iflag == 1)
    {
      printf("INF: hostname = %s\n", hostname);
      printf("INF: web_filename = %s\n", url_filename);
      printf("INF: output_filename = %s\n", local_filename);
    }

    /* connect the socket */
    connectsocket(hostname, &socket_descriptor);

    if (cflag == 1)
    {
      printf("REQ: GET %s HTTP/1.0\r\n", url_filename);
      printf("REQ: HOST: %s\r\n", hostname);
      printf("REQ: User-Agent: CWRU CSDS 325 Client 1.0\r\n");
    }

    /* make http request with socket */
    socket_pointer = fdopen(socket_descriptor, "r+");
    fprintf(socket_pointer, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", url_filename, hostname);

    /* get http status code from first line of response */
    fgets(buffer, BUFLEN, socket_pointer);
    strtok(buffer, " ");
    status_code = atoi(strtok(NULL, " "));

    if (sflag == 1)
    {
      printf("RSP: %s %i %s", buffer, status_code, strtok(NULL, ""));
    }

    /* read the header with fgets until we get a blank line */
    while (fgets(buffer, BUFLEN, socket_pointer) != NULL)
    {
      if (strcmp(buffer, "\r\n") == 0)
      {
        break;
      }
      if (sflag == 1)
      {
        printf("RSP: %s", buffer);
      }
      // reassign hostname and url_filename if -f flag is set,
      // status code is 301, and Location header is found
      if (fflag == 1 && status_code == 301 && (strncmp(buffer, "Location:", 9) == 0))
      {
        char *temp = (char *)malloc(strlen(buffer) + 1);
        strcpy(temp, buffer);
        strtok(temp, " ");
        char *new_url = strtok(NULL, " ");
        if (strncasecmp(new_url, "http://", 7) != 0)
        {
          errexit("ERROR: redirect hostname must start with http://", NULL);
        }
        hostname = strtok(new_url + 7, "/");
        char *rest = strtok(NULL, "\r\n");
        if (rest != NULL)
        {
          url_filename = malloc(strlen(rest) + 2);
          url_filename[0] = '/';
          strcpy(url_filename + 1, rest);
        }
        else
        {
          url_filename = "/";
        }
      }
    }

  } while (fflag == 1 && status_code == 301);

  if (status_code == 200)
  {
    FILE *file_pointer = fopen(local_filename, "w");
    if (file_pointer == NULL)
    {
      errexit("ERROR: cannot open file %s", local_filename);
    }

    /* read the body with fread to output_filename */
    int nbytes;
    while ((nbytes = fread(buffer, 1, BUFLEN, socket_pointer)) > 0)
    {
      fwrite(buffer, 1, nbytes, file_pointer);
    }
    fclose(file_pointer);

    /* close the socket */
    close(socket_descriptor);
  }
  else
  {
    errexit("ERROR: non-200 response code", NULL);
  }

  return 0;
}