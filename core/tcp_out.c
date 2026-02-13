#include "tcp_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TCP_MSS 1000 // 最大报文段长度 (Maximum Segment Size)

// 1. 底层输出：负责打包和发送，使用传入的 seq
void tcp_output(struct tcp_pcb *pcb, uint32_t seq, uint8_t flags, uint8_t *data, int len) {
    uint8_t buf[1500];
    struct my_tcp_hdr *tcph = (struct my_tcp_hdr *)buf;
    
    tcph->src_port = pcb->local_port;
    tcph->dst_port = pcb->remote_port;
    tcph->seq = htonl(seq);             // 使用传入的真实 Seq
    tcph->ack = htonl(pcb->rcv_nxt);
    tcph->rsvd_offset = (sizeof(struct my_tcp_hdr)/4) << 4;
    tcph->flags = flags;
    tcph->window = htons(pcb->rcv_wnd); // 通告本地的接收窗口
    
    if (len > 0) memcpy(buf + sizeof(struct my_tcp_hdr), data, len);
    
    tcph->cksum = tcp_calc_checksum(pcb, tcph, data, len);
    net_send(pcb, buf, sizeof(struct my_tcp_hdr) + len);
    
    printf("[OUT] Flags:0x%02X Seq:%u Len:%d\n", flags, seq, len);
}

// 2. 滑动窗口推流引擎 (核心)
void tcp_push(struct tcp_pcb *pcb) {
    // 飞行中的数据量 = 窗口右沿 - 窗口左沿
    uint32_t in_flight = pcb->snd_nxt - pcb->snd_una;
    // 剩余可用窗口 = 对方给的总窗口 - 已经发出去还没确认的
    int available_wnd = pcb->snd_wnd - in_flight;
    // 缓冲区里总共待发的数据量
    int total_unsent = rb_used_space(pcb->snd_buf);
    // 还没有发出的新数据量
    int new_data_len = total_unsent - in_flight;

    // 只要有新数据，且对方窗口允许，就循环发送
    while (new_data_len > 0 && available_wnd > 0) {
        int send_len = new_data_len;
        if (send_len > TCP_MSS) send_len = TCP_MSS;             // 不能超过 MSS
        if (send_len > available_wnd) send_len = available_wnd; // 不能超过可用窗口

        uint8_t buf[1500];
        // 关键：从环形缓冲区的偏移处窥探数据，不移动读指针！
        rb_peek_offset(pcb->snd_buf, in_flight, buf, send_len);

        tcp_output(pcb, pcb->snd_nxt, TCP_PSH | TCP_ACK, buf, send_len);

        pcb->snd_nxt += send_len; // 窗口右沿向前推
        in_flight += send_len;
        available_wnd -= send_len;
        new_data_len -= send_len;

        // 启动定时器
        if (pcb->timer_ms == 0) pcb->timer_ms = pcb->rto;
    }
}

// 3. 应用层写接口 (现在是非阻塞的)
void tcp_write(struct tcp_pcb *pcb, uint8_t *data, int len) {
    // 将数据塞入环形缓冲区
    int written = rb_write(pcb->snd_buf, data, len);
    if (written < len) {
        printf("[App] Warning: Send Buffer Full! Dropped %d bytes.\n", len - written);
    }
    // 触发推流引擎
    tcp_push(pcb);
}

// 4. 发送控制报文
void tcp_send_ctrl(struct tcp_pcb *pcb, uint8_t flags) {
    tcp_output(pcb, pcb->snd_nxt, flags, NULL, 0);
    if (flags & (TCP_SYN | TCP_FIN)) {
        pcb->snd_nxt++; // SYN/FIN 消耗一个序号
        if (pcb->timer_ms == 0) pcb->timer_ms = pcb->rto;
    }
}

// 5. 超时重传机制
void tcp_retransmit(struct tcp_pcb *pcb) {
    printf("[RTO] Timeout! Retransmitting from seq %u...\n", pcb->snd_una);
    
    // 只重传窗口最左边（snd_una）的一个 MSS 数据块
    int len = rb_used_space(pcb->snd_buf);
    if (len > 0) {
        if (len > TCP_MSS) len = TCP_MSS;
        uint8_t buf[1500];
        // 偏移 0：读取最老未确认的数据
        rb_peek_offset(pcb->snd_buf, 0, buf, len);
        tcp_output(pcb, pcb->snd_una, TCP_PSH | TCP_ACK, buf, len);
    } else {
        // 如果缓冲区为空，说明在途的是控制位 (SYN/FIN)
        if (pcb->state == TCP_SYN_SENT || pcb->state == TCP_SYN_RCVD) {
            tcp_output(pcb, pcb->snd_una, TCP_SYN, NULL, 0);
        } else {
            tcp_output(pcb, pcb->snd_una, TCP_FIN | TCP_ACK, NULL, 0);
        }
    }
    pcb->timer_ms = pcb->rto; // 重置定时器
}

// ... tcp_close 保持不变 ...
void tcp_close(struct tcp_pcb *pcb) {
    if (pcb->state == TCP_ESTABLISHED) {
        pcb->state = TCP_FIN_WAIT_1;
        tcp_send_ctrl(pcb, TCP_FIN | TCP_ACK);
    } else if (pcb->state == TCP_CLOSE_WAIT) {
        pcb->state = TCP_LAST_ACK;
        tcp_send_ctrl(pcb, TCP_FIN | TCP_ACK);
    }
}