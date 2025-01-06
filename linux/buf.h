#pragma once

typedef struct buf
{
    char *begin;
    char *end;
    char *curr;
} buf_t;



void buf_init(buf_t *, void *mem, size_t size);
void buf_reset(buf_t *);
buf_t *buf_alloc(size_t size);
// only for allocated with buf_alloc
void buf_free(buf_t **);
