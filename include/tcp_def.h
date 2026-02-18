#ifndef TCP_DEF_H
#define TCP_DEF_H

#include <stdint.h>
#include <netinet/in.h>
#include "ringbuf.h" // 必须引入环形缓冲区

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

// 统一定义最大报文段大小，方便各文件引用
#define TCP_MSS 1000
// [新增] RTO 最大限制为 60秒
#define TCP_MAX_RTO 60000

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
    
    // --- 发送端滑动窗口 (Tx Window) ---
    uint32_t snd_una;    // 窗口左沿 (最早未确认的序号)
    uint32_t snd_nxt;    // 窗口右沿 (下一个要发送的序号)
    uint32_t snd_wnd;    // 对方的接收窗口大小
    struct ringbuf *snd_buf; // 发送缓冲区

    // --- 拥塞控制 (Congestion Control) ---
    uint32_t cwnd;       // 拥塞窗口 (Congestion Window)
    uint32_t ssthresh;   // 慢启动阈值 (Slow Start Threshold)
    
    // --- 接收端滑动窗口 (Rx Window) ---
    uint32_t rcv_nxt;    // 期望收到的下一个序号
    uint32_t rcv_wnd;    // 本地的接收窗口大小
    struct ringbuf *rcv_buf; // 接收缓冲区
    
    // --- 定时器与控制 ---
    uint32_t timer_ms;       // 当前重传倒计时
    uint32_t rto;            // 超时时间设定 (ms)
};

#endif