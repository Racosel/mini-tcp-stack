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
    if (sendto(sockfd, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Net] Send failed");
    }
}

int net_recv(void *buf, int max_len) {
    // 接收包含 IP 头的数据
    return recvfrom(sockfd, buf, max_len, 0, NULL, NULL);
}