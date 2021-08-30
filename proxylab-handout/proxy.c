#include "cache.h"
#include "csapp.h"
#include "sbuf.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 4
#define SBUFSIZE 16

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

void doit(int fd);
void forward_requesthdrs(rio_t *rp, int fd, char *host_hdr);
int parse_url(char *url, char *host, char *port, char *uri);
void *thread(void *vargp);

sbuf_t sbuf;
cache_t cache;

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);

  listenfd = Open_listenfd(argv[1]);
  if(listenfd < 0) return;
  sbuf_init(&sbuf, SBUFSIZE);
  cache_init(&cache, 10);
  for (int i = 0; i < NTHREADS; ++i) {
    Pthread_create(&tid, NULL, thread, NULL);
  }

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    fprintf(stderr, "Accepted connection from (%s, %s)\n", hostname, port);
    fprintf(stderr, "slots: %d items: %d\n", sbuf.n - (sbuf.rear - sbuf.front),
            sbuf.rear - sbuf.front);
    sbuf_insert(&sbuf, connfd);
  }
  return 0;
}

void *thread(void *vargp) {
  Pthread_detach(pthread_self());
  while (1) {
    int connfd = sbuf_remove(&sbuf);
    doit(connfd);
    Close(connfd);
  }
}

void doit(int fd) {
  char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
  char uri[MAXLINE], host[MAXLINE], port[MAXLINE];
  rio_t rio_proxy, rio_web;
  int webserver_fd;

  P(&cache.timestamp_mutex);
  cache.timestamp++;
  V(&cache.timestamp_mutex);

  Rio_readinitb(&rio_proxy, fd);
  if (Rio_readlineb(&rio_proxy, buf, MAXLINE) < 0)
    return;

  sscanf(buf, "%s %s %s", method, url, version);
  fprintf(stderr, "%s", buf);
  if (strcmp(method, "GET")) {
    fprintf(stderr, "%s method not implemented!\n", method);
    return;
  }
  if (parse_url(url, host, port, uri) == -1) {
    return;
  }

  cache_ele *ele;
  if ((ele = cache_read(&cache, url)) == NULL) {
    // connect to web server
    webserver_fd = Open_clientfd(host, port);
    if(webserver_fd < 0) return;
    Rio_readinitb(&rio_web, webserver_fd);

    // host header
    strcat(host, ":");
    strcat(host, port);

    // send request line and headers from client
    sprintf(buf, "%s %s %s\r\n", method, uri, "HTTP/1.0");
    Rio_writen(webserver_fd, buf, strlen(buf));
    forward_requesthdrs(&rio_proxy, webserver_fd, host);

    // read response line and headers from web server
    int nbytes = 0, hsize = 0;
    char *response_hdrs = (char *)Malloc(MAX_OBJECT_SIZE);
    *response_hdrs = '\0';
    do {
      if ((nbytes = Rio_readlineb(&rio_web, buf, MAXLINE)) < 0)
        return;
      if (hsize + nbytes > MAX_OBJECT_SIZE)
        return;
      strcat(response_hdrs, buf);
      hsize += nbytes;
    } while (strcmp(buf, "\r\n"));
    //fprintf(stderr, "%s", response_hdrs);
    
    // read content from web server
    nbytes = 0;
    int object_read_size = 0;
    void *object = Malloc(MAX_OBJECT_SIZE);
    do {
      object_read_size += nbytes;
      if (nbytes == MAX_OBJECT_SIZE) {
        object = Realloc(object, object_read_size + MAX_OBJECT_SIZE);
      }
    } while ((nbytes = Rio_readnb(&rio_web, object + object_read_size,
                                  MAX_OBJECT_SIZE)) > 0);

    void *data = Realloc(response_hdrs, hsize + object_read_size);
    memcpy(data + hsize, object, object_read_size);
    Free(object);
    if (object_read_size <= MAX_OBJECT_SIZE) {
      cache_write(&cache, url, data, hsize + object_read_size);
    }
    Rio_writen(fd, data, hsize + object_read_size);
    Close(webserver_fd);
  } else {
    Rio_writen(fd, ele->data, ele->len);
  }
}

int parse_url(char *url, char *host, char *port, char *uri) {
  char *uri_ptr, *port_ptr;
  if (!strstr(url, "http://")) {
    fprintf(stderr, "Not valid http url!\n");
    return -1;
  }
  // jump over "http://"
  strcpy(host, url + 7);
  strcpy(port, "80");

  // parse port number and host
  if ((port_ptr = index(host, ':')) == 0) {
    uri_ptr = index(host, '/');
    *uri_ptr = '\0';
    uri_ptr++;
  } else {
    *port_ptr = '\0';
    port_ptr++;
    uri_ptr = index(port_ptr, '/');
    *uri_ptr = '\0';
    uri_ptr++;
    strcpy(port, port_ptr);
  }

  // get uri
  strcpy(uri, "/");
  strcat(uri, uri_ptr);
  return 0;
}

void forward_requesthdrs(rio_t *rp, int fd, char *host_hdr) {
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    if (strstr(buf, "Host")) {
      strcpy(host_hdr, buf);
    } else if (!strstr(buf, "User-Agent") && !strstr(buf, "Proxy-Connextion") &&
               !strstr(buf, "Proxy-Connection") && !strstr(buf, "Connection")) {
      if (Rio_writen(fd, buf, strlen(buf)) < 0)
        break;
    }
    if (Rio_readlineb(rp, buf, MAXLINE) < 0)
      break;
  }
  Rio_writen(fd, host_hdr, strlen(host_hdr));
  Rio_writen(fd, user_agent_hdr, strlen(user_agent_hdr));
  Rio_writen(fd, proxy_connection_hdr, strlen(proxy_connection_hdr));
  Rio_writen(fd, connection_hdr, strlen(connection_hdr));
  Rio_writen(fd, buf, strlen(buf)); // "\r\n"
}
