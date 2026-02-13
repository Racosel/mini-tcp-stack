#ifndef TCP_DEF_H
#define TCP_DEF_H

#include <stdint.h>
#include <netinet/in.h>
#include "ringbuf.h"

// TCP 状态枚举
typedef enum {
    TCP_CLOSED, TCP_LISTEN, TCP_SYN_SENT, TCP_SYN_RCVD,
    TCP_ESTABLISHED, TCP_FIN_WAIT_1, TCP_FIN_WAIT_2,
    TCP_CLOSING, TCP_TIME_WAIT, TCP_CLOSE_WAIT, TCP_LAST_ACK
} tcp_state_t;

// TCP 标志位
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

// 自定义 TCP 头部
struct my_tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  rsvd_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t cksum;
    uint16_t urg_ptr;
};

// 协议控制块 (PCB)
// include/tcp_def.h
// ... 保留前面的状态枚举和 tcp_hdr 结构体 ...

struct tcp_pcb {
    struct tcp_pcb *next; 

    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    
    tcp_state_t state;
    
    // --- 滑动窗口核心变量 ---
    uint32_t snd_una;    // Send Unacknowledged: 窗口左沿 (最早未确认的序号)
    uint32_t snd_nxt;    // Send Next: 窗口右沿 (下一个要发送的序号)
    uint32_t snd_wnd;    // Send Window: 对方的接收窗口大小
    
    uint32_t rcv_nxt;    // Receive Next: 期望收到的下一个序号
    uint32_t rcv_wnd;    // Receive Window: 本地的接收窗口大小
    
    // --- 环形缓冲区 ---
    struct ringbuf *snd_buf; // 发送缓冲区 (存放应用层写入、尚未确认的数据)
    struct ringbuf *rcv_buf; // 接收缓冲区 (任务 3.3 将使用)
    
    // --- 定时器 ---
    uint32_t timer_ms;       // 重传倒计时
    uint32_t rto;            // 超时时间设定 (ms)
};

#endif