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
                if (ack == pcb->snd_nxt) { // 之前发SYN已经 snd_nxt++，所以直接比对
                    pcb->state = TCP_ESTABLISHED;
                    pcb->snd_una = ack;     // 关键：握手成功，推进窗口左沿
                    pcb->rcv_nxt = seq + 1;
                    pcb->timer_ms = 0;      
                    
                    pcb->snd_wnd = ntohs(tcph->window); // 记录对方通知的窗口
                    
                    tcp_send_ctrl(pcb, TCP_ACK);
                    printf(">>> TCP Connection ESTABLISHED <<<\n");
                }
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_ACK) {
                // 1. 实时更新对方当前的接收能力
                pcb->snd_wnd = ntohs(tcph->window);
                
                // 2. 累计确认：如果 ACK 落在我们的窗口内
                if (ack > pcb->snd_una && ack <= pcb->snd_nxt) {
                    uint32_t acked_bytes = ack - pcb->snd_una;
                    printf("[ACK] %u bytes acked. Rem Wnd: %u\n", acked_bytes, pcb->snd_wnd);
                    
                    // 从环形缓冲区中“吃掉”已被确认的数据 (释放空间)
                    int in_buf = rb_used_space(pcb->snd_buf);
                    int to_pop = (acked_bytes > in_buf) ? in_buf : acked_bytes; // 防止把 SYN/FIN 也当数据吃掉
                    
                    if (to_pop > 0) {
                        uint8_t dummy[1500];
                        while(to_pop > 0) {
                            int chunk = to_pop > 1500 ? 1500 : to_pop;
                            rb_read(pcb->snd_buf, dummy, chunk); // 真实移动 tail 指针
                            to_pop -= chunk;
                        }
                    }
                    
                    // 3. 推进窗口左沿
                    pcb->snd_una = ack;
                    
                    // 4. 定时器管理
                    if (pcb->snd_una == pcb->snd_nxt) {
                        pcb->timer_ms = 0; // 全确认了，关定时器
                    } else {
                        pcb->timer_ms = pcb->rto; // 还有在途数据，重置
                    }
                    
                    // 5. 窗口可能腾出空间了，尝试继续推流
                    tcp_push(pcb);
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