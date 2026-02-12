#include "tcp_internal.h"
#include <stdio.h>

void tcp_input(uint8_t *buf, int len) {
    // 1. 跳过 IP 头 (IHL * 4)
    int ip_hdr_len = (buf[0] & 0x0F) * 4;
    struct my_tcp_hdr *tcph = (struct my_tcp_hdr *)(buf + ip_hdr_len);

    // 2. 查找 PCB (DstIP=Local, SrcIP=Remote)
    struct tcp_pcb *pcb = tcp_pcb_find(0, tcph->dst_port, 0, tcph->src_port);
    
    if (!pcb) {
        return; // 未监听端口，忽略
    }

    // 3. 状态机处理
    tcp_process_state(pcb, tcph, len - ip_hdr_len);
}