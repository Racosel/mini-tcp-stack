#include "tcp_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void tcp_output(struct tcp_pcb *pcb, uint8_t flags, uint8_t *data, int len) {
    uint8_t buf[1500];
    struct my_tcp_hdr *tcph = (struct my_tcp_hdr *)buf;
    
    tcph->src_port = pcb->local_port;
    tcph->dst_port = pcb->remote_port;
    tcph->seq = htonl(pcb->snd_nxt);
    tcph->ack = htonl(pcb->rcv_nxt);
    tcph->rsvd_offset = (sizeof(struct my_tcp_hdr)/4) << 4;
    tcph->flags = flags;
    tcph->window = htons(4096);
    
    if (len > 0) memcpy(buf + sizeof(struct my_tcp_hdr), data, len);
    
    // 计算校验和
    tcph->cksum = tcp_calc_checksum(pcb, tcph, data, len);
    
    // 发送
    net_send(pcb, buf, sizeof(struct my_tcp_hdr) + len);
    printf("[OUT] Flags:0x%02X Seq:%u Len:%d\n", flags, pcb->snd_nxt, len);
}

void tcp_write(struct tcp_pcb *pcb, uint8_t *data, int len) {
    if (pcb->waiting_ack) return; // 阻塞

    if (pcb->retrans_buf) free(pcb->retrans_buf);
    pcb->retrans_buf = malloc(len);
    memcpy(pcb->retrans_buf, data, len);
    pcb->retrans_len = len;
    pcb->retrans_flags = TCP_PSH | TCP_ACK;
    
    pcb->waiting_ack = 1;
    pcb->timer_ms = pcb->rto;
    
    tcp_output(pcb, TCP_PSH | TCP_ACK, data, len);
}

void tcp_send_ctrl(struct tcp_pcb *pcb, uint8_t flags) {
    tcp_output(pcb, flags, NULL, 0);
    // SYN 和 FIN 消耗序列号并需要重传
    if (flags & (TCP_SYN | TCP_FIN)) {
        pcb->waiting_ack = 1;
        pcb->retrans_len = 0;
        pcb->retrans_flags = flags;
        pcb->timer_ms = pcb->rto;
    }
}

void tcp_retransmit(struct tcp_pcb *pcb) {
    printf("[RTO] Timeout! Retransmitting...\n");
    tcp_output(pcb, pcb->retrans_flags, pcb->retrans_buf, pcb->retrans_len);
    pcb->timer_ms = pcb->rto;
}

void tcp_close(struct tcp_pcb *pcb) {
    if (pcb->state == TCP_ESTABLISHED) {
        printf("[Close] Active Close.\n");
        pcb->state = TCP_FIN_WAIT_1;
        tcp_send_ctrl(pcb, TCP_FIN | TCP_ACK);
    } else if (pcb->state == TCP_CLOSE_WAIT) {
        printf("[Close] Passive Close.\n");
        pcb->state = TCP_LAST_ACK;
        tcp_send_ctrl(pcb, TCP_FIN | TCP_ACK);
    }
}