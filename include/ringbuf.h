#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdlib.h>

struct ringbuf {
    uint8_t *buf;   // 实际的数据存储区
    int size;       // 缓冲区的总容量
    int head;       // 写指针 (Write Index)
    int tail;       // 读指针 (Read Index)
    int count;      // 当前缓冲区内的数据量
};

// 分配并初始化缓冲区
struct ringbuf *rb_new(int size);

// 释放缓冲区内存
void rb_free(struct ringbuf *rb);

// 写入数据，返回实际写入的字节数
int rb_write(struct ringbuf *rb, uint8_t *data, int len);

// 读取并移除数据，返回实际读取的字节数
int rb_read(struct ringbuf *rb, uint8_t *out, int len);

// 从指定的 offset 处查看数据（不移动读指针，常用于发送窗口内的数据重发）
int rb_peek_offset(struct ringbuf *rb, int offset, uint8_t *out, int len);

// 获取剩余空闲空间 (用于通告接收窗口 rcv_wnd)
int rb_free_space(struct ringbuf *rb);

// 获取已使用空间 (用于计算有多少数据可以发送)
int rb_used_space(struct ringbuf *rb);

#endif