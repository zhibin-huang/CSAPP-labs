#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

typedef struct {
  char tag[MAXLINE];
  int last_visit;
  int reader_cnt;
  int len;
  void *data;
  sem_t mutex;
  sem_t w;
} cache_ele;

typedef struct {
  int n;
  unsigned int timestamp;
  sem_t timestamp_mutex;
  sem_t eviction;
  cache_ele *buf;
} cache_t;

void cache_init(cache_t *cp, int n);

void cache_deinit(cache_t *cp);

cache_ele *cache_read(cache_t *c, char *key);

void cache_write(cache_t *c, char *key, void *data, int len);

void write_ele(cache_t *cache, int index, char *key, void *data, int len);

#endif
