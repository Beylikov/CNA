#include "emulator.h"
#include "gbn.h"

/* 参数定义 */
#define RTT        16.0
#define WINDOWSIZE 6
#define SEQSPACE   12
#define NOTINUSE   (-1)

/* A 端接口（空壳） */
void A_init(void)              {}
void A_output(message msg)     {}
void A_input(packet pkt)       {}
void A_timerinterrupt(void)    {}

/* B 端接口（空壳） */
void B_init(void)              {}
void B_input(packet pkt)       {}
void B_output(message msg)     {}
void B_timerinterrupt(void)    {}
