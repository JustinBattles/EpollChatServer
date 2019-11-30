#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define _POSIX_C_SOURC >= 199506L
#define K	1024
#define PORT	47350

int key = PORT;

int watch(int epfd, int fd)
{
  struct epoll_event e;
  // No need to add EPOLLERR or EPOLLHUP, but you can:
  e.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
  e.data.fd = fd;
  
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e) < 0) {
    perror("epoll_ctl");
    exit(1);
  }
  return 0;
}


void combobulate(uint8_t *buf, size_t size) {
  int key = 47308;
  for (int i = 0; i < size; i++) {
   buf[i] ^= (rand_r(&key) & 0xFF);
  }
 
}

ssize_t enc_write(int fd, void *buf, size_t size) {
  combobulate(buf, size);
  return write(fd, buf, size);

}

size_t enc_read(int fd, void *buf, size_t size) {
  int r = read(fd, buf, size);
  if (r > 0) combobulate(buf, r);
  return r;
}


/**
 * chat <hostname> [<port>]
 */
int main(int argc, char *argv[])
{
  char *username = getenv("USER");
  struct epoll_event ev[2];
  struct sockaddr_in addr;
  struct hostent *he;
  char buf[K], out[K*2];
  int sock, epfd, r, n, i, done = 0;
  unsigned short port = PORT;


  if (argc < 2) {
    printf("Usage: chat <hostname> [<port>]\n");
    exit(0);
  }
  char *hostname = argv[1];
  if (argc > 2) port = atoi(argv[2]);
  
  if ((epfd = epoll_create(1)) < 0) {
    perror("epoll_create");
    exit(1);
  }

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if ((he = gethostbyname(hostname)) == NULL) {
    herror("gethostbyname");
    exit(1);
  }
  memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
  
  if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
    perror("connect");
    exit(1);
  }
  
  watch(epfd, STDIN_FILENO);
  watch(epfd, sock);

  printf("connected.\n");
  while(!done)  {
    n = epoll_wait(epfd, ev, 2, -1);
    if (n < 0) {
      perror("epoll_wait");
      continue;
    }
    for(i = 0; i < n; i++) {
      if (ev[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {printf("error1\n"); done = 1; break; }

      if (ev[i].data.fd == STDIN_FILENO) {
	r = read(STDIN_FILENO, buf, K);
	if (r <= 0) {printf("error2\n");  done=1; break;}
	int k=0, o=0;
	for(;k < r; k++) {
	  if (buf[k] == '\n') out[o++] = '\r';
	  out[o++] = buf[k];
	}
	enc_write(sock, out, o);
	//write(sock, out, o);
      }
      if (ev[i].data.fd == sock) {
//	r = read(sock, buf, K);
	r = enc_read(sock, buf, K);
	if (r <= 0) {printf("error3\n");  done=1; break; }
	write(STDOUT_FILENO, buf, r);
      }
    }
  }
  return 0;
}
