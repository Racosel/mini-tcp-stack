#include "tcp_internal.h"

void tcp_timer_tick(struct tcp_pcb *pcb, int ms_elapsed) {
    if (pcb->waiting_ack && pcb->timer_ms > 0) {
        if (pcb->timer_ms <= ms_elapsed) {
            tcp_retransmit(pcb);
        } else {
            pcb->timer_ms -= ms_elapsed;
        }
    }
}