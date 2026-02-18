#include <stdlib.h>
#include <string.h>
#include "tcp_internal.h"

static struct tcp_pcb *pcb_list = NULL;

struct tcp_pcb *tcp_pcb_new() {
    struct tcp_pcb *pcb = malloc(sizeof(struct tcp_pcb));
    memset(pcb, 0, sizeof(struct tcp_pcb));
    pcb->rto = 1000; // 默认 1秒超时
    
    // 初始化环形缓冲区 (4KB)
    pcb->snd_buf = rb_new(4096);
    pcb->rcv_buf = rb_new(4096);
    
    // --- 初始化拥塞控制参数 ---
    pcb->cwnd = TCP_MSS;      // 初始只发 1 个 MSS (慢启动)
    pcb->ssthresh = 0xFFFF;   // 初始阈值设为无穷大 (65535)

    // [新增] 初始化 RTT 变量
    pcb->rto = 1000;   // 在获得第一个采样前，保持默认 1000ms
    pcb->rtt_ts = 0;   // 0 表示当前没有进行 RTT 测量
    pcb->srtt = 0;
    pcb->rttvar = 0;

    // --- [新增] 初始化乱序链表为空 ---
    pcb->ooo_head = NULL; 

    // --- [新增] 初始化坚持定时器 ---
    pcb->persist_timer_ms = 0;
    pcb->persist_backoff = 1;

    pcb->rcv_nxt = 0;

    // 初始化窗口大小
    pcb->rcv_wnd = rb_free_space(pcb->rcv_buf); // 动态获取
    pcb->snd_wnd = 1024; // 初始假设对方有窗口，握手后更新
    
    // 插入头部
    pcb->next = pcb_list;
    pcb_list = pcb;
    return pcb;
}

struct tcp_pcb *tcp_pcb_find(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport) {
    struct tcp_pcb *pcb = pcb_list;
    while (pcb != NULL) {
        // 简单匹配：这里只匹配目的端口，实际应匹配四元组
        if (pcb->local_port == lport) {
            return pcb;
        }
        pcb = pcb->next;
    }
    return NULL;
}