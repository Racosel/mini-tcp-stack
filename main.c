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
    FILE *fp = NULL;
    int file_eof = 0; // 0:未读完, 1:已读完但在等ACK, 2:已关闭

    FILE *fp_recv = fopen("test_recv.dat", "wb"); 
    if (!fp_recv) {
        perror("[App] Failed to open test_recv.dat");
    }
    
    while(1) {
        // 1. 非阻塞 I/O 接收 (彻底抽干内核缓冲区)
        while (1) {
            int len = net_recv(buf, 1500);
            if (len <= 0) break; // 没包了，立刻退出 while，绝不死等！
            tcp_input(buf, len);
        }
        
        // 2. 定时器驱动 (由于没被阻塞，定时器现在极其精准)
        tcp_timer_tick(pcb, 10);

        // --- [新增] 4.3 测试专用的接收抽水机 ---
        if (pcb->state == TCP_ESTABLISHED || pcb->state == TCP_CLOSE_WAIT) {
            int unread_len = rb_used_space(pcb->rcv_buf);
            if (unread_len > 0) {
                uint8_t read_buffer[4096];
                int to_read = (unread_len > sizeof(read_buffer)) ? sizeof(read_buffer) : unread_len;
                
                // 从内核接收缓冲区读出数据，腾出底层窗口
                rb_read(pcb->rcv_buf, read_buffer, to_read);
                
                // 写入文件并刷盘
                if (fp_recv) {
                    fwrite(read_buffer, 1, to_read, fp_recv);
                    fflush(fp_recv);
                }
                
                // 【极度关键】抽走数据后，回复纯 ACK 更新窗口
                tcp_output(pcb, pcb->snd_nxt, TCP_ACK, NULL, 0);
            }
        }
        
        // --- 3. 业务逻辑：流式读取并发送文件 ---
        // if (pcb->state == TCP_ESTABLISHED && file_eof == 0) {
        //     // 懒加载打开文件
        //     if (!fp) {
        //         fp = fopen("test_send.dat", "rb");
        //         if (!fp) {
        //             perror("[App] Failed to open test_send.dat");
        //             file_eof = -1; 
        //         } else {
        //             printf("[App] Started sending 1MB file...\n");
        //         }
        //     }
            
        //     if (fp) {
        //         // 检查发送缓冲区还有多少剩余空间
        //         int free_space = rb_free_space(pcb->snd_buf);
        //         if (free_space > 0) {
        //             uint8_t file_buf[1000];
        //             // 每次最多读 1000B，且不超过缓冲区剩余空间
        //             int to_read = free_space > sizeof(file_buf) ? sizeof(file_buf) : free_space;
        //             int n = fread(file_buf, 1, to_read, fp);
                    
        //             if (n > 0) {
        //                 tcp_write(pcb, file_buf, n); // 写入缓冲区并触发推流
        //             } else if (feof(fp)) {
        //                 printf("[App] EOF reached. Waiting for all packets to be ACKed...\n");
        //                 file_eof = 1;
        //                 fclose(fp);
        //             }
        //         }
        //     }
        // }
        
        // --- 4. 优雅关闭：确保文件发完且全部被确认 ---
        if (file_eof == 1 && pcb->state == TCP_ESTABLISHED) {
            // 如果缓冲区空了，且最后一个包都被确认了 (snd_una == snd_nxt)
            if (rb_used_space(pcb->snd_buf) == 0 && pcb->snd_una == pcb->snd_nxt) {
                printf("[App] All 1MB data successfully ACKed! Initiating active close.\n");
                tcp_close(pcb);
                file_eof = 2; 
            }
        }
        
        // 被动关闭处理
        if (pcb->state == TCP_CLOSE_WAIT) {
            printf("App: Peer closed, closing now...\n");
            tcp_close(pcb);
        }

        if (fp_recv) {
            fclose(fp_recv);
            fp_recv = NULL;
        }

        usleep(10000); // 10ms
    }
    return 0;
}