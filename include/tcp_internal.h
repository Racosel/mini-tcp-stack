#ifndef TCP_INTERNAL_H
#define TCP_INTERNAL_H

#include "tcp_def.h"

// core/tcp_pcb.c
struct tcp_pcb *tcp_pcb_new();
struct tcp_pcb *tcp_pcb_find(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport);

// core/tcp_in.c
void tcp_input(uint8_t *buf, int len);

// core/tcp_out.c
void tcp_output(struct tcp_pcb *pcb, uint8_t flags, uint8_t *data, int len);
void tcp_send_ctrl(struct tcp_pcb *pcb, uint8_t flags);
void tcp_write(struct tcp_pcb *pcb, uint8_t *data, int len);
void tcp_retransmit(struct tcp_pcb *pcb);
void tcp_close(struct tcp_pcb *pcb);

// core/tcp_state.c
void tcp_process_state(struct tcp_pcb *pcb, struct my_tcp_hdr *tcph, int len);

// core/tcp_timer.c
void tcp_timer_tick(struct tcp_pcb *pcb, int ms_elapsed);

// adapter/net_io.c
void net_init();
void net_send(struct tcp_pcb *pcb, void *buf, int len);
int net_recv(void *buf, int max_len);

// utils/checksum.c
uint16_t tcp_calc_checksum(struct tcp_pcb *pcb, struct my_tcp_hdr *tcph, uint8_t *data, int len);

#endif