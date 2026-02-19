#include "tcp_internal.h"
#include <stdio.h>

// 处理接收到的数据负载
static void tcp_process_payload(struct tcp_pcb *pcb, uint32_t seq, uint8_t *data, int len) {
    if (len <= 0) return;

    if (seq == pcb->rcv_nxt) {
        // 按序到达，写入接收缓冲区
        int written = rb_write(pcb->rcv_buf, data, len);
        if (written > 0) {
            pcb->rcv_nxt += written;
            pcb->rcv_wnd = rb_free_space(pcb->rcv_buf); // 更新接收窗口
            printf("[Data] Accepted %d bytes. Next Exp: %u, Win: %u\n", written, pcb->rcv_nxt, pcb->rcv_wnd);
        }
        tcp_send_ctrl(pcb, TCP_ACK); // 回复 ACK
    } 
    else if (seq < pcb->rcv_nxt) {
        // 重复包
        printf("[Data] Duplicate Seq %u < Exp %u. Resending ACK.\n", seq, pcb->rcv_nxt);
        tcp_send_ctrl(pcb, TCP_ACK);
    }
    else {
        // 乱序包 (直接丢弃，回 ACK 触发对方重传)
        printf("[Data] Out-of-order Seq %u > Exp %u. Dropping.\n", seq, pcb->rcv_nxt);
        tcp_send_ctrl(pcb, TCP_ACK); 
    }
}

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

    switch (pcb->state) {
        case TCP_SYN_SENT:
            if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
                if (ack == pcb->snd_nxt) {
                    pcb->state = TCP_ESTABLISHED;
                    pcb->snd_una = ack; 
                    pcb->rcv_nxt = seq + 1;
                    pcb->timer_ms = 0;
                    pcb->snd_wnd = ntohs(tcph->window); // 记录对方窗口
                    tcp_send_ctrl(pcb, TCP_ACK);
                    printf(">>> TCP Connection ESTABLISHED <<<\n");
                }
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_ACK) {
                pcb->snd_wnd = ntohs(tcph->window);
                if (ack > pcb->snd_una && ack <= pcb->snd_nxt) {
                    uint32_t acked = ack - pcb->snd_una;
                    
                    // --- 【修复开始】分块读取，防止栈溢出 ---
                    int to_pop = (acked > (uint32_t)rb_used_space(pcb->snd_buf)) ? rb_used_space(pcb->snd_buf) : acked;
                    while (to_pop > 0) {
                        uint8_t dummy[1500];
                        int chunk = (to_pop > 1500) ? 1500 : to_pop;
                        rb_read(pcb->snd_buf, dummy, chunk);
                        to_pop -= chunk;
                    }
                    // --- 【修复结束】 ---


                    // --- [新增] 拥塞控制状态机 ---
                    if (pcb->cwnd < pcb->ssthresh) {
                        // 1. 慢启动阶段 (Slow Start): 指数增长
                        // 每收到一个 ACK，cwnd 增加一个 MSS
                        pcb->cwnd += TCP_MSS;
                        printf("[Congestion] Slow Start: cwnd -> %u\n", pcb->cwnd);
                    } else {
                        // 2. 拥塞避免阶段 (Congestion Avoidance): 线性增长
                        // 每个 RTT 增加一个 MSS。
                        // 近似算法：每收到一个 ACK，增加 MSS * MSS / cwnd
                        uint32_t increment = (TCP_MSS * TCP_MSS) / pcb->cwnd;
                        if (increment < 1) increment = 1; // 至少加 1 字节
                        pcb->cwnd += increment;
                        printf("[Congestion] Avoidance: cwnd -> %u\n", pcb->cwnd);
                    }
                    // ---------------------------
                    
                    pcb->snd_una = ack;
                    pcb->timer_ms = (pcb->snd_una == pcb->snd_nxt) ? 0 : pcb->rto;
                    tcp_push(pcb);
                }
            }
            
            // 处理接收数据
            if (payload_len > 0) {
                if (seq == pcb->rcv_nxt) {
                    tcp_process_payload(pcb, seq, payload, payload_len);
                } else {
                    // --- [极其关键的兜底] ---
                    // 无论是收到乱序包，还是 Linux 发来的零窗口试探包(旧包)，
                    // 必须无条件向对方回复最新的 ACK 和 Window！
                    tcp_output(pcb, pcb->snd_nxt, TCP_ACK, NULL, 0);
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
            if ((flags & TCP_ACK) && (ack == pcb->snd_nxt)) {
                pcb->snd_una = ack;
                pcb->timer_ms = 0;
                pcb->state = TCP_FIN_WAIT_2;
            }
            if (flags & TCP_FIN) {
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
            if ((flags & TCP_ACK) && (ack == pcb->snd_nxt)) {
                pcb->snd_una = ack;
                pcb->state = TCP_CLOSED;
                printf(">>> CLOSED <<<\n");
            }
            break;
            
        case TCP_CLOSE_WAIT:
            break;
            
        default:
            break;
    }
}