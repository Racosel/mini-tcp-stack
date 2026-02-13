#include "tcp_internal.h"

void tcp_timer_tick(struct tcp_pcb *pcb, int ms_elapsed) {
    // 只要有未确认的数据，定时器就生效
    if (pcb->snd_una != pcb->snd_nxt && pcb->timer_ms > 0) {
        if (pcb->timer_ms <= ms_elapsed) {
            tcp_retransmit(pcb);
        } else {
            pcb->timer_ms -= ms_elapsed;
        }
    }
}