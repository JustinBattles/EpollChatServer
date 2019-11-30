#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>


/* includes */

#define K	1024
#define PORT	47308

int clients[K], firstFree = 5;
int head = 4;
int authenticated[K];
int *keys[K];

void initArrays() {
int key = 47308;
 for (int i = 0; i < K; i++) {
   clients[i] = -1;
   authenticated[i] = 0;
   keys[i] = &key;
 }

}

int getClient(int fd) {

 for (int i = head; i < firstFree; i++) {
	 printf("Matching %d element with %d fd\n", i, fd);
   if (clients[i] == fd)
     return i;
 }

 return -1;

}


void combobulate(int *key, uint8_t *buf, size_t size) {
  fprintf(stderr, "KEY: %d\n", key);
  for (int i = 0; i < size; i++) {
    buf[i] ^= (rand_r(key) & 0xFF);
  }
}

ssize_t enc_write(int fd, void *buf, size_t size, int *key) {
  combobulate(key, buf, size);
  return write(fd, buf, size); 
}

size_t enc_read(int fd, void *buf, size_t size, int *key) {
  int r = read(fd, buf, size);
  if (r > 0) combobulate(key, buf, r);
 return r; 
}

int login(int sock) {
 char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

 char *pos, username[K], password[K];

 if ((pos = strrchr(username, '\n'))) *pos = '\0';
 if ((pos = strrchr(password, '\n'))) *pos = '\0';

 FILE *fp = fopen("auth.db", "r");
 if (fp == NULL) {
   perror("fopen");
   exit(1);
 }

 char *usr, *hash, salt[K], *pwd, buf[K];

 while(fgets(buf, K, fp) != NULL) {
   usr = strtok(buf, ":");
   hash = strtok(NULL, "\r\n");
   if (strcmp(usr, username) == 0) {
     strcpy(salt, hash);
     pos = strrchr(salt, '$');
     if (pos) *pos = '\0';
     else break;
     pwd = crypt(password, salt);
     if (strcmp(pwd, hash) == 0) {
       fclose(fp);
       return 1;
     } else {
       fclose(fp);
       return 0;
     }
   }
 }


}
void newClient(int fd) {

  clients[firstFree] = fd;
  firstFree++;

//  login(fd);

}



void broadcast(int from, char *msg, size_t len) {

	fprintf(stderr,"Key Array: \n");

	for (int i = head; i < firstFree; i++)
		fprintf(stderr, "{%d} Key: %d\n", i, keys[i]);
  for (int i = head; i < firstFree; i++) {
    if (clients[i] != from && clients[i] != 0) {
	    int client_pos = getClient(i);
	    int *key = keys[client_pos];
	    printf("Key here: %d\n", keys[client_pos]);
     if (enc_write(clients[i], msg, len, key) < 0) {
        clients[i] = 0;
	//fprintf(stderr, "{%d}, key: %d\n", i, keys[i]);
      }
    }
  }
}

int watch(int epfd, int fd) {

  struct epoll_event ev = {.events = EPOLLIN|EPOLLOUT, .data.fd = fd};
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
    perror("epoll_ctl");	
}

int main() {

  int epfd, n;
  struct epoll_event epv[10];
  char buf[K];
  int r;

  initArrays();

  int sock  = socket(AF_INET, SOCK_STREAM, 0);

  int option = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
	  perror("setsockopt");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);

  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
    perror("bind");
    exit(1); 
  }

  if ((epfd = epoll_create(1)) < 0) {
    perror("epoll-create");
    exit(1);
  }

  watch(epfd, sock);
if (listen(sock, 128) < 0) {
    perror("listen");
    exit(0);
  }
  int events = 1;

  for (;;) {
    n = epoll_wait(epfd, epv, events, -1);
    if (n <= -1) {
      perror("epoll wait");
      break;
    }

  for (int i = 0; i < n; i++) {
	  if (epv[i].events & (EPOLLHUP | EPOLLERR)) break;
	   if (epv[i].data.fd == sock) {
      socklen_t client_len = sizeof(struct sockaddr_in);
      struct sockaddr_in client_addr;
      
      int client_sock = accept(sock, (struct sockaddr*) &client_addr, &client_len);
    watch(epfd, client_sock);

     newClient(client_sock);
      events++;
      for (int i = head; i < firstFree; i++) {
         fprintf(stderr, "{%d}, fd: %d firstFree: %d\n", i, clients[i], firstFree);
         fprintf(stderr, "{%d}, key: %d\n", i, keys[i]);
      }
      
      fprintf(stderr, "Server: Connection from descriptor %d on address %s\n", client_sock, inet_ntoa(client_addr.sin_addr));
     
     }
     else if (epv[i].events & EPOLLIN) {
        int client_pos = getClient(epv[i].data.fd);        
        r = enc_read(epv[i].data.fd, buf, K, keys[client_pos]);
      	write(STDOUT_FILENO, buf, r);
	     for (int j = 0; j < n; j++) {
	     //if (epv[j].data.fd != sock && epv[j].data.fd != i)
		     broadcast(epv[j].data.fd, buf, r);
      	    //enc_write(epv[j].data.fd, buf, r, keys[client_pos]);
        }
      }
    }
  }
}
