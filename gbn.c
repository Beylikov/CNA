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
/* 计算并返回 pkt 的校验和 */
unsigned short ComputeChecksum(packet *pkt) {
    unsigned long sum = 0;
    unsigned short *data = (unsigned short *)pkt;
    for (int i = 0; i < sizeof(packet)/2; i++) {
        sum += data[i];
        if (sum & 0xFFFF0000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    return (unsigned short)(~sum);
}

/* 判断 pkt 是否损坏 */
bool IsCorrupted(packet *pkt) {
    return ComputeChecksum(pkt) != pkt->checksum;
}
/* A 端状态变量 */
static int base, nextseqnum;
static packet sender_buffer[WINDOWSIZE];
static double timer[WINDOWSIZE];

/* 初始化 A */
void A_init(void) {
    base = nextseqnum = 0;
    for (int i = 0; i < WINDOWSIZE; i++) {
        timer[i] = NOTINUSE;
    }
}

/* 应用层调用，发送新消息 */
void A_output(message msg) {
    packet pkt;
    pkt.seqnum  = nextseqnum;
    pkt.acknum  = NOTINUSE;
    memcpy(pkt.payload, msg.data, sizeof(msg.data));
    pkt.checksum = ComputeChecksum(&pkt);

    sender_buffer[nextseqnum % WINDOWSIZE] = pkt;
    if (nextseqnum < base + WINDOWSIZE) {
        tolayer3(AEntity, pkt);
        timer[nextseqnum % WINDOWSIZE] = RTT;
    }
    nextseqnum++;
}

/* 接收到 ACK */
void A_input(packet rcvd) {
    if (!IsCorrupted(&rcvd) &&
        rcvd.acknum >= base && rcvd.acknum < nextseqnum) {
        int shift = rcvd.acknum - base + 1;
        for (int i = 0; i < shift; i++) {
            timer[(base + i) % WINDOWSIZE] = NOTINUSE;
        }
        base = rcvd.acknum + 1;
    }
}

/* 定时器中断：重发窗口内所有超时包 */
void A_timerinterrupt(void) {
    for (int i = base; i < nextseqnum; i++) {
        if (timer[i % WINDOWSIZE] == 0) {
            tolayer3(AEntity, sender_buffer[i % WINDOWSIZE]);
            timer[i % WINDOWSIZE] = RTT;
        }
    }
}
