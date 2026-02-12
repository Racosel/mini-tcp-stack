#include <stdlib.h>
#include <string.h>
#include "tcp_internal.h"

static struct tcp_pcb *pcb_list = NULL;

struct tcp_pcb *tcp_pcb_new() {
    struct tcp_pcb *pcb = malloc(sizeof(struct tcp_pcb));
    memset(pcb, 0, sizeof(struct tcp_pcb));
    pcb->rto = 1000; // 默认 1秒超时
    
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