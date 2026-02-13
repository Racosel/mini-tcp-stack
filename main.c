#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include "tcp_internal.h"

int main() {
    net_init();
    
    struct tcp_pcb *pcb = tcp_pcb_new();
    
    // --- Mininet 配置 ---
    // h1 (本机)
    pcb->local_ip = inet_addr("10.0.0.1"); 
    pcb->local_port = htons(12345);
    // h2 (目标机)
    pcb->remote_ip = inet_addr("10.0.0.2"); 
    pcb->remote_port = htons(8080);
    
    pcb->state = TCP_SYN_SENT;
    
    printf("Starting TCP Stack on 10.0.0.1:12345 -> 10.0.0.2:8080\n");
    
    // 1. 发起握手
    tcp_send_ctrl(pcb, TCP_SYN);
    
    uint8_t buf[1500];
    int tick = 0;
    
    while(1) {
        // IO
        int len = net_recv(buf, 1500);
        if (len > 0) tcp_input(buf, len);
        
        // Timer
        tcp_timer_tick(pcb, 10);
        
        // 业务逻辑
        /*
        if (pcb->state == TCP_ESTABLISHED) {
            if (tick % 200 == 0 && tick < 1000) {
                char msg[50];
                sprintf(msg, "Hello from MiniTCP tick=%d", tick);
                tcp_write(pcb, (uint8_t*)msg, strlen(msg));
            }
            
            // 运行一段时间后主动关闭
            if (tick == 1000) {
                tcp_close(pcb);
            }
        }
        */
        // [3.2] Tx 测试点：发送大数据量，触发拆分发送
        static int data_sent = 0;
        if (pcb->state == TCP_ESTABLISHED && !data_sent) {
            // 只要一进入 ESTABLISHED 状态，立刻发送
            char big_data[2500];
            memset(big_data, 'A', 2500); 
            tcp_write(pcb, (uint8_t*)big_data, 2500);
            data_sent = 1; // 确保只发一次
        }
        
        // 被动关闭响应
        if (pcb->state == TCP_CLOSE_WAIT) {
            printf("App: Peer closed, closing now...\n");
            tcp_close(pcb);
        }
        
        usleep(10000); // 10ms
        tick++;
    }
    return 0;
}