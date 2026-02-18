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
    
    pcb->state = TCP_SYN_SENT;
    printf("Starting TCP Stack on 10.0.0.1:12345 -> 10.0.0.2:8080\n");
    
    tcp_send_ctrl(pcb, TCP_SYN);
    
    uint8_t buf[1500];
    FILE *fp_send = NULL; // 原来的 fp 改名为 fp_send 显得更清晰
    
    // --- [新增] 准备接收文件的指针 ---
    FILE *fp_recv = fopen("recv_from_h2.dat", "wb"); 
    if (!fp_recv) {
        perror("[App] Failed to open recv_from_h2.dat");
        // 即使打开失败也可以继续，只是不落盘而已
    }

    int file_eof = 0; // 0:未读完, 1:已读完但在等ACK, 2:已发送完毕关闭
    
    while(1) {
        // 1. 非阻塞 I/O 接收 (彻底抽干内核缓冲区)
        while (1) {
            int len = net_recv(buf, 1500);
            if (len <= 0) break; // 没包了，立刻退出 while，绝不死等！
            tcp_input(buf, len);
        }
        
        // 2. 定时器驱动
        tcp_timer_tick(pcb, 10);
        
        // --- 3. 业务逻辑：流式读取并发送文件 (h1 -> h2) ---
        if (pcb->state == TCP_ESTABLISHED && file_eof == 0) {
            if (!fp_send) {
                fp_send = fopen("test_send.dat", "rb");
                if (!fp_send) {
                    perror("[App] Failed to open test_send.dat");
                    file_eof = -1; 
                } else {
                    printf("[App] Started sending 1MB file...\n");
                }
            }
            
            if (fp_send) {
                int free_space = rb_free_space(pcb->snd_buf);
                if (free_space > 0) {
                    uint8_t file_buf[1000];
                    int to_read = free_space > sizeof(file_buf) ? sizeof(file_buf) : free_space;
                    int n = fread(file_buf, 1, to_read, fp_send);
                    
                    if (n > 0) {
                        tcp_write(pcb, file_buf, n);
                    } else if (feof(fp_send)) {
                        printf("[App] EOF reached. Waiting for all packets to be ACKed...\n");
                        file_eof = 1;
                        fclose(fp_send);
                        fp_send = NULL;
                    }
                }
            }
        }
        
        // --- [新增] 5. 业务逻辑：流式接收文件并落盘 (h2 -> h1) ---
        if (pcb->state == TCP_ESTABLISHED || pcb->state == TCP_CLOSE_WAIT) {
            int unread_len = rb_used_space(pcb->rcv_buf);
            
            if (unread_len > 0 && fp_recv) {
                uint8_t read_buffer[4096];
                int to_read = (unread_len > sizeof(read_buffer)) ? sizeof(read_buffer) : unread_len;
                
                // 从 TCP 接收缓冲区读出数据（这会腾出底层接收窗口 pcb->rcv_wnd）
                rb_read(pcb->rcv_buf, read_buffer, to_read);
                
                // 写入磁盘文件
                fwrite(read_buffer, 1, to_read, fp_recv);
                fflush(fp_recv); // 强制刷盘，防止进程异常退出时数据丢失
                
                // [极度关键] 应用层读走数据，窗口变大了，必须发一个纯 ACK 通知 h2 更新窗口 (Window Update)！
                // 否则如果之前窗口满了，h2 就会死锁等待
                tcp_output(pcb, pcb->snd_nxt, TCP_ACK, NULL, 0);
            }
        }

        // --- 4. 优雅关闭：确保发送文件发完且全部被确认 ---
        if (file_eof == 1 && pcb->state == TCP_ESTABLISHED) {
            if (rb_used_space(pcb->snd_buf) == 0 && pcb->snd_una == pcb->snd_nxt) {
                printf("[App] All 1MB data successfully ACKed! Initiating active close.\n");
                tcp_close(pcb);
                file_eof = 2; 
            }
        }
        
        // 被动关闭处理
        if (pcb->state == TCP_CLOSE_WAIT) {
            printf("[App] Peer closed, closing now...\n");
            tcp_close(pcb);
        }

        // --- [新增] 6. 彻底退出机制 ---
        if (pcb->state == TCP_CLOSED) {
            printf("[App] Connection fully closed. Exiting.\n");
            break; // 跳出主循环，结束进程
        }
        
        usleep(10000); // 10ms
    }

    if (fp_recv) fclose(fp_recv);
    return 0;
}