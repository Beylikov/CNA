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
