#include "ringbuf.h"

struct ringbuf *rb_new(int size) {
    struct ringbuf *rb = malloc(sizeof(struct ringbuf));
    rb->buf = malloc(size);
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return rb;
}

void rb_free(struct ringbuf *rb) {
    if (rb) {
        if (rb->buf) free(rb->buf);
        free(rb);
    }
}

int rb_write(struct ringbuf *rb, uint8_t *data, int len) {
    int free_spc = rb->size - rb->count;
    if (len > free_spc) len = free_spc; // 防止溢出，只能写入剩余空间的量

    for (int i = 0; i < len; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
    }
    rb->count += len;
    return len;
}

int rb_read(struct ringbuf *rb, uint8_t *out, int len) {
    if (len > rb->count) len = rb->count; // 只能读取已有的数据量

    for (int i = 0; i < len; i++) {
        out[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
    }
    rb->count -= len;
    return len;
}

int rb_peek_offset(struct ringbuf *rb, int offset, uint8_t *out, int len) {
    if (offset >= rb->count) return 0; // 偏移超出了现有数据量
    if (offset + len > rb->count) len = rb->count - offset; // 截断超出的部分
    
    int start_idx = (rb->tail + offset) % rb->size;
    for (int i = 0; i < len; i++) {
        out[i] = rb->buf[(start_idx + i) % rb->size];
    }
    return len;
}

int rb_used_space(struct ringbuf *rb) {
    return rb->count;
}

int rb_free_space(struct ringbuf *rb) {
    return rb->size - rb->count;
}