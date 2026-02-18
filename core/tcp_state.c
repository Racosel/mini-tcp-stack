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

                // --- 情况 A: 收到新的 ACK (New ACK) ---
                if (ack > pcb->snd_una && ack <= pcb->snd_nxt) {
                    // [新增] 只要收到新数据确认，重置 dupacks 计数
                    pcb->dupacks = 0;

                    // --- [新增] 动态 RTT 估算与 RTO 更新 (Jacobson/Karels 算法) ---
                    // 如果正在测量，且当前收到的 ACK 涵盖了我们测量的包
                    if (pcb->rtt_ts != 0 && ack >= pcb->rtt_seq) {
                        uint32_t sample_rtt = sys_now() - pcb->rtt_ts;
                        pcb->rtt_ts = 0; // 测量完成，允许测下一个包

                        if (pcb->srtt == 0) {
                            // 第一次测量
                            pcb->srtt = sample_rtt;
                            pcb->rttvar = sample_rtt / 2;
                        } else {
                            // 后续平滑计算 (避免浮点数，使用移位/乘除近似)
                            // RTTVAR = (3/4)*RTTVAR + (1/4)*|SRTT - Sample|
                            uint32_t delta = (pcb->srtt > sample_rtt) ? (pcb->srtt - sample_rtt) : (sample_rtt - pcb->srtt);
                            pcb->rttvar = (3 * pcb->rttvar + delta) / 4;
                            // SRTT = (7/8)*SRTT + (1/8)*Sample
                            pcb->srtt = (7 * pcb->srtt + sample_rtt) / 8;
                        }
                        
                        printf("[RTT] Sample: %u ms, SRTT: %u ms, RTTVAR: %u\n", sample_rtt, pcb->srtt, pcb->rttvar);
                    }

                    // ... (保留 4.1/4.2 原有的处理逻辑：计算 acked, 更新 cwnd, 释放 buf, 重置 RTO 等) ...
                    // 请确保把你之前写的 4.1(拥塞控制) 和 4.2(RTO重置) 的代码都保留在下面这个块里
                    {
                        uint32_t acked = ack - pcb->snd_una;
                        
                        // 拥塞控制状态机 (4.1 代码)
                        if (pcb->cwnd < pcb->ssthresh) {
                            pcb->cwnd += TCP_MSS; 
                        } else {
                            uint32_t inc = (TCP_MSS * TCP_MSS) / pcb->cwnd;
                            pcb->cwnd += (inc < 1) ? 1 : inc;
                        }

                        // 释放缓冲区 (3.3 代码)
                        int to_pop = (acked > (uint32_t)rb_used_space(pcb->snd_buf)) ? rb_used_space(pcb->snd_buf) : acked;
                        while(to_pop > 0) {
                             uint8_t d[1500];
                             int c = (to_pop > 1500) ? 1500 : to_pop;
                             rb_read(pcb->snd_buf, d, c);
                             to_pop -= c;
                        }

                        pcb->snd_una = ack;
                        
                        // --- [修改] 移除 4.2 里写死的 pcb->rto = 1000，改为动态健康 RTO ---
                        uint32_t healthy_rto = (pcb->srtt == 0) ? 1000 : (pcb->srtt + 4 * pcb->rttvar);
                        if (healthy_rto < TCP_MIN_RTO) healthy_rto = TCP_MIN_RTO;
                        if (healthy_rto > TCP_MAX_RTO) healthy_rto = TCP_MAX_RTO;
                        
                        if (pcb->rto != healthy_rto) {
                            pcb->rto = healthy_rto;
                            // printf("[Timer] RTO adjusted to %u ms\n", pcb->rto); // 可选：打印查看
                        }
                        
                        pcb->timer_ms = (pcb->snd_una == pcb->snd_nxt) ? 0 : pcb->rto;
                        tcp_push(pcb);
                    }
                } 
                // --- [新增] 情况 B: 收到重复 ACK (Duplicate ACK) ---
                else if (ack == pcb->snd_una) {
                    // 只有当窗口内有数据未确认时，DupACK 才有意义
                    if (rb_used_space(pcb->snd_buf) > 0) {
                        pcb->dupacks++;
                        printf("[Fast ReTx] DupACK detected (%d/3). Seq: %u\n", pcb->dupacks, ack);

                        // 触发快速重传阈值
                        if (pcb->dupacks == 3) {
                            printf(">>> [Fast ReTx] TRIGGERED! Retransmitting seq %u immediately! <<<\n", ack);
                            
                            // [新增] 快速重传也属于重传，废弃当前的 RTT 测量
                            pcb->rtt_ts = 0;
                            
                            // 1. 立即重传丢失的包 (复用 tcp_retransmit)
                            // 注意：这里复用 tcp_retransmit 会副作用导致 cwnd=1 (Tahoe 算法行为)
                            // 虽然 Reno 算法建议 cwnd 减半，但在学生实现中，直接重传最稳健。
                            tcp_retransmit(pcb);
                            
                            // 2. 关键：重置定时器，防止 RTO 再次触发导致双重重传
                            pcb->timer_ms = pcb->rto;
                        }
                    }
                }
            }

            // 处理接收数据
            if (payload_len > 0) {
                tcp_process_payload(pcb, seq, payload, payload_len);
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