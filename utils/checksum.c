#include "tcp_internal.h"

uint16_t tcp_calc_checksum(struct tcp_pcb *pcb, struct my_tcp_hdr *tcph, uint8_t *data, int len) {
    uint32_t sum = 0;
    
    // 1. 伪首部
    sum += (pcb->local_ip >> 16) + (pcb->local_ip & 0xFFFF);
    sum += (pcb->remote_ip >> 16) + (pcb->remote_ip & 0xFFFF);
    sum += htons(IPPROTO_TCP);
    sum += htons(sizeof(struct my_tcp_hdr) + len);
    
    // 2. TCP 头部 (校验和字段先置0)
    tcph->cksum = 0;
    uint16_t *p = (uint16_t*)tcph;
    for(int i=0; i<sizeof(struct my_tcp_hdr)/2; i++) sum += p[i];
    
    // 3. 数据部分
    p = (uint16_t*)data;
    while(len > 1) { sum += *p++; len -= 2; }
    if(len) sum += *(uint8_t*)p; // 处理奇数尾字节
    
    // 4. 折叠溢出
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}