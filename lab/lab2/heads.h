#pragma once
#include <stdio.h>
#include <winsock2.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#define REQUEST_SUCCEEDED 0
#define REQUEST_FAILED 1
#define INIT_FAILED 2
#define INIT_SUCCEEDED 3
#define STAGE_FINISHED 4
#define STAGE_ERROR 5
#define BUFFER_SIZE 1026 //缓冲区大小，（以太网中 UDP 的数据帧中包长度应小于 1480 字节）
#define SERVER_PORT 12340 // port
#define TIMEOUT_COUNT 10 //超时计数
#define SEND_WIND_SIZE 10 //发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
#define RECV_WIND_SIZE 10 //发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
#define PACK_SIZE 1024 //每个包的大小为 1024 字节
//本例取序列号 0...19 共 20 个
//如果将窗口大小设为 1，则为停-等协议
#define SEQ_SIZE 20 //序列号的个数，从 0~19 共计 20 个
//由于发送数据第一个字节如果值为 0，则数据会发送失败
//因此接收端序列号为 1~20，与发送端一一对应
#define DATA_SIZE (PACK_SIZE * 113)
#define SERVER_IP "127.0.0.1" // 服务器的 IP 地址

#define BEGIN_TEST_INFO 100
#define EXIT_TEST_INFO 101
#define GBN_REQUEST_INFO 200
#define GBN_RESPONSE_INFO 201
#define GBN_CONFORM_INFO 202
#define TANSFER_FINISHED_INFO 203
#define TRANSFER_FINISHED_ACK_INFO 204

#define SENT 10
#define ACKED 11
#define NEEDTORESEND 12

float packetLossRatio = 0.2; 
float ackLossRatio = 0.2;

typedef uint8_t ERROR_CODE;

int SOCKADDR_Length = sizeof(SOCKADDR);