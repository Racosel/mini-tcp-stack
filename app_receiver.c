#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "tcp_internal.h"

int main() {
    net_init();
    
    struct tcp_pcb *pcb = tcp_pcb_new();
    pcb->local_ip = inet_addr("10.0.0.1"); 
    pcb->local_port = htons(12345);
    pcb->remote_ip = inet_addr("10.0.0.2"); 
    pcb->remote_port = htons(8080);
    
    // 1. 准备接收文件
    FILE *fp_recv = fopen("test_recv.dat", "wb"); 
    if (!fp_recv) {
        perror("[App FATAL] Failed to open test_recv.dat for writing!");
        return -1; // 如果文件打不开，直接退出，绝不让你白等
    }
    printf("[App] File test_recv.dat opened successfully. Ready to receive.\n");

    // 2. 发起主动连接 (握手)
    pcb->state = TCP_SYN_SENT;
    printf("[App] Starting TCP Stack: 10.0.0.1:12345 -> 10.0.0.2:8080\n");
    tcp_send_ctrl(pcb, TCP_SYN);
    
    uint8_t buf[1500];
    int total_bytes_written = 0; // 统计总共写入了多少字节
    
    while(1) {
        // --- A. 底层非阻塞接收网络包 ---
        while (1) {
            int len = net_recv(buf, 1500);
            if (len <= 0) break; 
            tcp_input(buf, len);
        }
        
        // --- B. 定时器驱动 ---
        tcp_timer_tick(pcb, 10);
        
        // --- C. 核心业务逻辑：应用层抽水机与落盘 ---
        if (pcb->state == TCP_ESTABLISHED || pcb->state == TCP_CLOSE_WAIT) {
            int unread_len = rb_used_space(pcb->rcv_buf);
            
            if (unread_len > 0) {
                uint8_t read_buffer[4096];
                int to_read = (unread_len > sizeof(read_buffer)) ? sizeof(read_buffer) : unread_len;
                
                // 从底层环形缓冲区抽走数据
                int actual_read = rb_read(pcb->rcv_buf, read_buffer, to_read);
                
                if (actual_read > 0) {
                    // 写入硬盘！
                    size_t w = fwrite(read_buffer, 1, actual_read, fp_recv);
                    fflush(fp_recv); // 强制刷盘，防止残留在 C 标准库缓冲区
                    
                    total_bytes_written += w;
                    
                    // 终极监控打印：让你亲眼看着文件变大！
                    printf("[App PUMP] Extracted %d bytes. Wrote %zu bytes to disk. (Total: %d bytes)\n", 
                           actual_read, w, total_bytes_written);
                }
                
                // 极度关键：通知 Linux 窗口已腾出空间
                tcp_output(pcb, pcb->snd_nxt, TCP_ACK, NULL, 0);
            }
        }
        
        // --- D. 优雅关闭逻辑 ---
        if (pcb->state == TCP_CLOSE_WAIT) {
            printf("[App] Peer sent FIN. Closing connection...\n");
            tcp_close(pcb);
        }
        
        if (pcb->state == TCP_CLOSED) {
            printf("[App] Connection fully closed. Exiting main loop.\n");
            break;
        }
        
        usleep(10000); // 10ms 睡眠，防止 CPU 100% 空转
    }

    // 3. 安全收尾
    if (fp_recv) {
        fclose(fp_recv);
        printf("[App] File flushed and closed. Total file size should be: %d bytes.\n", total_bytes_written);
    }
    
    return 0;
}