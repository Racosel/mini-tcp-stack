// app_sender.c
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "tcp_internal.h"

int main() {
    net_init();
    struct tcp_pcb *pcb = tcp_pcb_new();
    pcb->local_ip = inet_addr("10.0.0.1"); 
    pcb->local_port = htons(12345);
    pcb->remote_ip = inet_addr("10.0.0.2"); 
    pcb->remote_port = htons(8080);
    
    FILE *fp_send = fopen("test_send.dat", "rb");
    if (!fp_send) { perror("File open failed"); return -1; }

    pcb->state = TCP_SYN_SENT; // 同样是主动连接
    tcp_send_ctrl(pcb, TCP_SYN);
    
    uint8_t buf[1500];
    int file_eof = 0;
    while(1) {
        while (1) {
            int len = net_recv(buf, 1500);
            if (len <= 0) break; 
            tcp_input(buf, len);
        }
        tcp_timer_tick(pcb, 10);
        
        // --- 核心：只发不收 ---
        if (pcb->state == TCP_ESTABLISHED && !file_eof) {
            int free_space = rb_free_space(pcb->snd_buf);
            if (free_space > 0) {
                uint8_t tmp[4096];
                int to_read = free_space > 4096 ? 4096 : free_space;
                int actual = fread(tmp, 1, to_read, fp_send);
                if (actual > 0) {
                    tcp_write(pcb, tmp, actual); // 塞入发送缓冲区
                } else if (feof(fp_send)) {
                    printf("[Sender] All data loaded into send buffer.\n");
                    file_eof = 1; 
                }
            }
        }
        
        // 发送完毕且被全部确认后关闭
        if (file_eof && pcb->snd_una == pcb->snd_nxt && pcb->state == TCP_ESTABLISHED) {
            printf("[Sender] All data ACKed. Closing...\n");
            tcp_close(pcb);
        }
        
        if (pcb->state == TCP_CLOSED) break;
        usleep(10000); 
    }
    fclose(fp_send);
    return 0;
}