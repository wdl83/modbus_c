#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "check.h"
#include "log.h"

void buf_init(buf_t *buf, void *p, size_t size)
{
    if(!buf || !p || !size) return;
    buf->begin = p;
    buf->end = buf->begin + size;
    buf->curr = buf->begin;
}

void buf_reset(buf_t *buf)
{
    if(buf) memset(buf, 0, sizeof(buf_t));
}

buf_t *buf_alloc(size_t size)
{
    void *p = malloc(sizeof(buf_t) + size);
    CHECK_ERRNO(p);
    memset(p, 0, sizeof(buf_t) + size);
    buf_t *buf = (buf_t *)p;
    buf->begin = p + sizeof(buf_t);
    buf->end = buf->begin + size;
    buf->curr = buf->begin;
    logT("%p %zu", buf, size);
    return buf;
}

void buf_free(buf_t **pbuf)
{
    if(!pbuf || !*pbuf) return;

    free(*pbuf);
    logT("%p", *pbuf);
    *pbuf = NULL;
}
