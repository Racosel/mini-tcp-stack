#include "tcp_internal.h"
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

static int sockfd = -1;

void net_init() {
    // 创建 Raw Socket，只接收 TCP
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("[Net] Socket init failed (Root required?)");
        exit(1);
    }
}

void net_send(struct tcp_pcb *pcb, void *buf, int len) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = pcb->remote_ip;
    
    // 发送原始数据包 (包含 TCP 头和数据)
    int ret = sendto(sockfd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("[Fatal] sendto failed"); // 如果内核拒绝发包，会在这里打印原因（比如 Network is unreachable）
    }
}

int net_recv(void *buf, int max_len) {
    // 加上 MSG_DONTWAIT，没包立刻返回 -1，绝不卡死！
    return recvfrom(sockfd, buf, max_len, MSG_DONTWAIT, NULL, NULL);
}