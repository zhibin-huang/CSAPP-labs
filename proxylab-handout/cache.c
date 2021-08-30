#include "cache.h"
#include "csapp.h"

void cache_init(cache_t *cp, int n) {
  cp->timestamp = 0;
  cp->n = n;
  Sem_init(&(cp->timestamp_mutex), 0, 1);
  Sem_init(&(cp->eviction), 0, 1);
  cp->buf = (cache_ele *)Malloc(sizeof(cache_ele) * n);
  for (int i = 0; i < n; ++i) {
    cache_ele *e = &(cp->buf[i]);
    e->last_visit = 0;
    e->reader_cnt = 0;
    e->data = NULL;
    Sem_init(&e->mutex, 0, 1);
    Sem_init(&e->w, 0, 1);
    memset(e->tag, 0, sizeof(e->tag));
  }
}

void cache_deinit(cache_t *cp) {
  for (int i = 0; i < cp->n; ++i) {
    Free(cp->buf[i].data);
    cp->buf[i].data = NULL;
  }
  Free(cp->buf);
  cp->buf = NULL;
}

cache_ele *cache_read(cache_t *cache, char *key) {
  for (int i = 0; i < cache->n; ++i) {
    cache_ele *ele = &(cache->buf[i]);
    P(&ele->mutex);
    ele->reader_cnt++;
    if (ele->reader_cnt == 1) {
      P(&ele->w);
    }
    V(&ele->mutex);

    if (strcmp(ele->tag, key) == 0) {
      ele->last_visit = cache->timestamp;
      return ele;
    }

    P(&ele->mutex);
    ele->reader_cnt--;
    if (ele->reader_cnt == 0) {
      V(&ele->w);
    }
    V(&ele->mutex);
  }
  return NULL;
}

void cache_write(cache_t *cache, char *key, void *data, int len) {
  P(&cache->eviction);
  int minn = 0x7fffffff, j = 0;
  for (int i = 0; i < cache->n; ++i) {
    cache_ele *ele = &(cache->buf[i]);
    if (ele->data == NULL) {
      write_ele(cache, i, key, data, len);
      V(&cache->eviction);
      return;
    }
    if (ele->last_visit < minn) {
      minn = ele->last_visit;
      j = i;
    }
  }
  write_ele(cache, j, key, data, len);
  V(&cache->eviction);
}

void write_ele(cache_t *cache, int index, char *key, void *data, int len) {
  cache_ele *ele = &(cache->buf[index]);
  P(&ele->w);
  if (ele->data) {
    Free(ele->data);
  }
  ele->data = Malloc(sizeof(char) * len);
  memcpy(ele->data, data, len);
  strcpy(ele->tag, key);
  ele->len = len;
  ele->last_visit = cache->timestamp;

  V(&ele->w);
}
