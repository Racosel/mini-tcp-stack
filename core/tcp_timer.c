#include "tcp_internal.h"
#include <stdio.h>
#include <sys/time.h> // [新增] 引入 gettimeofday
#include <stddef.h>

// [新增] 获取当前毫秒级时间戳
uint32_t sys_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void tcp_timer_tick(struct tcp_pcb *pcb, int ms_elapsed) {
    if (pcb->snd_una != pcb->snd_nxt && pcb->timer_ms > 0) {
        if (pcb->timer_ms <= ms_elapsed) {
            // [新增] 1. 指数退避：RTO 翻倍
            pcb->rto *= 2;
            if (pcb->rto > TCP_MAX_RTO) pcb->rto = TCP_MAX_RTO;
            
            // [新增] 2. 打印调试信息，观察 RTO 变化
            printf("[Timer] Timeout reached! Backing off RTO -> %u ms\n", pcb->rto);

            // 3. 调用重传 (tcp_retransmit 会使用这个新的 pcb->rto 重置 timer_ms)
            tcp_retransmit(pcb);
        } else {
            pcb->timer_ms -= ms_elapsed;
        }
    }
}