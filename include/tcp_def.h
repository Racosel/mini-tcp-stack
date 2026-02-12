#ifndef TCP_DEF_H
#define TCP_DEF_H

#include <stdint.h>
#include <netinet/in.h>

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
struct tcp_pcb {
    struct tcp_pcb *next; 

    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    
    tcp_state_t state;
    
    // 序列号
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    
    // 重传缓存
    uint8_t *retrans_buf;
    uint16_t retrans_len;
    uint8_t  retrans_flags;
    
    // 定时器与控制
    int      waiting_ack;    // 1=停等阻塞中
    uint32_t timer_ms;       // 当前倒计时
    uint32_t rto;            // 超时时间设定 (ms)
};

#endif