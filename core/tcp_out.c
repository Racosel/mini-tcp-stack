#include "tcp_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TCP_MSS 1000

// 1. 底层发送
void tcp_output(struct tcp_pcb *pcb, uint32_t seq, uint8_t flags, uint8_t *data, int len) {
    uint8_t buf[1500];
    struct my_tcp_hdr *tcph = (struct my_tcp_hdr *)buf;
    
    tcph->src_port = pcb->local_port;
    tcph->dst_port = pcb->remote_port;
    tcph->seq = htonl(seq);                 
    tcph->ack = htonl(pcb->rcv_nxt);
    tcph->rsvd_offset = (sizeof(struct my_tcp_hdr)/4) << 4;
    tcph->flags = flags;
    
    // 关键：动态通告本地当前的接收窗口 (Flow Control)
    pcb->rcv_wnd = rb_free_space(pcb->rcv_buf);
    tcph->window = htons((uint16_t)pcb->rcv_wnd);
    
    if (len > 0) memcpy(buf + sizeof(struct my_tcp_hdr), data, len);
    
    tcph->cksum = tcp_calc_checksum(pcb, tcph, data, len);
    net_send(pcb, buf, sizeof(struct my_tcp_hdr) + len);
    
    printf("[OUT] Flags:0x%02X Seq:%u Len:%d Wnd:%u\n", flags, seq, len, pcb->rcv_wnd);
}

// 2. 推流引擎
void tcp_push(struct tcp_pcb *pcb) {
    uint32_t in_flight = pcb->snd_nxt - pcb->snd_una; 
    int available_wnd = pcb->snd_wnd - in_flight; 
    int unsent = rb_used_space(pcb->snd_buf) - in_flight; 

    // 只要有数据且对方窗口允许，就一直发
    while (unsent > 0 && available_wnd > 0) {
        int send_len = unsent;
        if (send_len > TCP_MSS) send_len = TCP_MSS;       
        if (send_len > available_wnd) send_len = available_wnd; 

        uint8_t buf[1500];
        rb_peek_offset(pcb->snd_buf, in_flight, buf, send_len);

        tcp_output(pcb, pcb->snd_nxt, TCP_PSH | TCP_ACK, buf, send_len);

        pcb->snd_nxt += send_len;
        in_flight += send_len;
        unsent -= send_len;
        available_wnd -= send_len;

        if (pcb->timer_ms == 0) pcb->timer_ms = pcb->rto;
    }
}

// 3. 应用层写接口
void tcp_write(struct tcp_pcb *pcb, uint8_t *data, int len) {
    int written = rb_write(pcb->snd_buf, data, len);
    if (written < len) {
        printf("[App] Warning: Send buffer full! Dropped %d bytes.\n", len - written);
    }
    tcp_push(pcb); // 写完立即尝试推流
}

// 4. 发送控制包 (SYN/FIN)
void tcp_send_ctrl(struct tcp_pcb *pcb, uint8_t flags) {
    tcp_output(pcb, pcb->snd_nxt, flags, NULL, 0);
    if (flags & (TCP_SYN | TCP_FIN)) {
        pcb->snd_nxt++; 
        if (pcb->timer_ms == 0) pcb->timer_ms = pcb->rto;
    }
}

// 5. 超时重传
void tcp_retransmit(struct tcp_pcb *pcb) {
    printf("[RTO] Timeout! Retransmitting from seq %u...\n", pcb->snd_una);
    int len = rb_used_space(pcb->snd_buf);
    
    if (len > 0) {
        if (len > TCP_MSS) len = TCP_MSS;
        uint8_t buf[1500];
        rb_peek_offset(pcb->snd_buf, 0, buf, len);
        tcp_output(pcb, pcb->snd_una, TCP_PSH | TCP_ACK, buf, len);
    } else {
        if (pcb->state == TCP_SYN_SENT || pcb->state == TCP_SYN_RCVD) {
            tcp_output(pcb, pcb->snd_una, TCP_SYN, NULL, 0);
        } else {
            tcp_output(pcb, pcb->snd_una, TCP_FIN | TCP_ACK, NULL, 0);
        }
    }
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