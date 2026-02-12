#include "tcp_internal.h"
#include <stdio.h>

void tcp_process_state(struct tcp_pcb *pcb, struct my_tcp_hdr *tcph, int len) {
    uint32_t seq = ntohl(tcph->seq);
    uint32_t ack = ntohl(tcph->ack);
    uint8_t flags = tcph->flags;
    int hdr_len = (tcph->rsvd_offset >> 4) * 4;
    uint8_t *payload = (uint8_t *)tcph + hdr_len;
    int payload_len = len - hdr_len;

    if (flags & TCP_RST) {
        printf("[RST] Connection Reset.\n");
        pcb->state = TCP_CLOSED;
        return;
    }

    printf("[State] %d | Recv Flags:0x%02X Seq:%u Ack:%u\n", pcb->state, flags, seq, ack);

    switch (pcb->state) {
        case TCP_SYN_SENT:
            if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
                if (ack == pcb->snd_nxt + 1) {
                    pcb->state = TCP_ESTABLISHED;
                    pcb->snd_nxt++; 
                    pcb->rcv_nxt = seq + 1;
                    pcb->waiting_ack = 0;
                    tcp_send_ctrl(pcb, TCP_ACK);
                    printf(">>> ESTABLISHED <<<\n");
                }
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_ACK) {
                uint32_t expect = pcb->snd_nxt + (pcb->retrans_len > 0 ? pcb->retrans_len : 0);
                if (ack == expect) {
                    pcb->snd_nxt = ack;
                    pcb->waiting_ack = 0;
                    pcb->timer_ms = 0;
                }
            }
            if (payload_len > 0) {
                if (seq == pcb->rcv_nxt) {
                    printf("[Data] Recv: %.*s\n", payload_len, payload);
                    pcb->rcv_nxt += payload_len;
                    tcp_send_ctrl(pcb, TCP_ACK);
                } else {
                    tcp_send_ctrl(pcb, TCP_ACK); // 乱序重发ACK
                }
            }
            if (flags & TCP_FIN) {
                printf("[FIN] Peer closed.\n");
                pcb->rcv_nxt = seq + 1;
                tcp_send_ctrl(pcb, TCP_ACK);
                pcb->state = TCP_CLOSE_WAIT;
            }
            break;
            
        case TCP_FIN_WAIT_1:
            if ((flags & TCP_ACK) && (ack == pcb->snd_nxt + 1)) {
                pcb->snd_nxt++;
                pcb->waiting_ack = 0;
                pcb->state = TCP_FIN_WAIT_2;
            }
            if (flags & TCP_FIN) { // Simultaneous close
                 pcb->rcv_nxt = seq + 1;
                 tcp_send_ctrl(pcb, TCP_ACK);
                 pcb->state = TCP_CLOSING;
            }
            break;

        case TCP_FIN_WAIT_2:
            if (flags & TCP_FIN) {
                pcb->rcv_nxt = seq + 1;
                tcp_send_ctrl(pcb, TCP_ACK);
                pcb->state = TCP_TIME_WAIT;
                printf("[TIME_WAIT] Connection closed.\n");
            }
            break;

        case TCP_LAST_ACK:
            if ((flags & TCP_ACK) && (ack == pcb->snd_nxt + 1)) {
                pcb->snd_nxt++;
                pcb->state = TCP_CLOSED;
                printf(">>> CLOSED <<<\n");
            }
            break;
            
        case TCP_CLOSE_WAIT:
            // 等待应用层调用 tcp_close()
            break;
            
        default:
            break;
    }
}