/*******************************************************************************
* Copyright (c) 2017, Alan Barr
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "rtsp.h"

typedef struct {
    int socket;
    uint32_t ip;
} ClientData;


int rtspServerStatus = 0;

RtspStatus sendData(RtspClientRx *client, const uint8_t *buffer, uint16_t size);
RtspStatus control(RtspSession *session, RtspSessionInstruction instruction);

static const char * const sdp = 
        "v=0\r\n"
        "o=derp 1 1 IN IPV4 192.168.1.178\r\n"
        "s=derp\r\n"
        "t=0 0\r\n"
        "m=audio 49170 RTP/AVP 98\r\n"
        "a=rtpmap:98 L16/16000/18\r\n";

RtspStatus sendData(RtspClientRx *client, const uint8_t *buffer, uint16_t size)
{
    int rtn = 0;
    ClientData *clientData = (ClientData*)client;

    printf("In Tx callback. Size is %u\n", size);

    rtn = write(clientData->socket, buffer, size);

    if (rtn < 0)
    {
        fprintf(stderr, "write failed.\n");
        return RTSP_ERROR_CALLBACK;
    }
    return 0;
}

RtspStatus control(RtspSession *session,
                   RtspSessionInstruction instruction)
{
    const char *string = NULL;

    switch (instruction)
    {
        case RTSP_SESSION_CREATE:
            string = "create";
            break;
        case RTSP_SESSION_PLAY:
            string = "play";
            break;
        case RTSP_SESSION_PAUSE:
            string = "pause";
            break;
        case RTSP_SESSION_DELETE:
            string = "delete";
            break;
    }
    printf("==========\n RTSP session control event. Session %p instruction %s\n",
           session,
           string);

    return RTSP_OK;
}

int startRtsp(void)
{
    RtspConfig config;
    RtspResource resource;
    RtspResource *resourceHandle;
    const char *string = "";

    memset(&config, 0, sizeof(config));
    config.callback.tx = sendData;
    config.callback.control = control;
    if (RTSP_OK != rtspInit(&config))
    {
        return -1;
    }

    memset(&resource, 0, sizeof(resource));

    strcpy(resource.path.data, string);
    resource.path.length = strlen(string);

    resource.sdp.data    = sdp;
    resource.sdp.length  = strlen(sdp);

    resource.sdp.length  = strlen(sdp);
    resource.serverPortRange[0] = 9998;
    resource.serverPortRange[1] = 9999;

    if (RTSP_OK != rtspResourceInit(&resource, &resourceHandle))
    {
        return -1;
    }

    return 0;
}

int rtpServerThead(pthread_t *rtpServerThread)
{
    return 0;
}

void* rtspServer(void *arg)
{
    int serverSock;
    int readSize;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    int clientAddrLength;
    char rxBuffer[2000];
    ClientData client;
    time_t currentTime;
    const int port = 13001;
    (void)arg;

    printf("Creating socket.\n");
    serverSock = socket(AF_INET, SOCK_STREAM, 0);

    time(&currentTime);

    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_addr.s_addr  = INADDR_ANY;
    serverAddr.sin_port         = htons(port + (currentTime & 0X1));

    if (bind(serverSock,
             (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) < 0)
    {
        perror("bind failed. Error");
        return NULL;
    }

    printf("Listening.\n");
    if (listen(serverSock, 1))
    {
        perror("listen failed.");
        return NULL;
    }

    printf("Waiting for incoming connections on port %u\n", 
            htons(serverAddr.sin_port));
    clientAddrLength = sizeof(clientAddr);

    while (1)
    {
        client.socket = accept(serverSock,
                               (struct sockaddr *)&clientAddr,
                               (socklen_t*)&clientAddrLength);

        if (clientAddr.sin_family != AF_INET)
        {
            perror("Unexpected client protocol");
            return NULL;
        }

        if (client.socket < 0)
        {
            perror("accept failed");
            return NULL;
        }

        printf("Connection accepted\n");

        clientAddr.sin_addr.s_addr = htonl(clientAddr.sin_addr.s_addr);
        printf("Client information: %u.%u.%u.%u:%u\n", 
                                (clientAddr.sin_addr.s_addr & 0xFF000000)>>24,
                                (clientAddr.sin_addr.s_addr & 0x00FF0000)>>16,
                                (clientAddr.sin_addr.s_addr & 0x0000FF00)>>8,
                                (clientAddr.sin_addr.s_addr & 0x000000FF)>>0,
                                clientAddr.sin_port);

        while (1)
        {
            readSize = recv(client.socket, rxBuffer, 2000, 0);

            if (readSize < 0)
            {
                fprintf(stderr, "recv() returned error.\n");
                break;
            }

            if (readSize == 0)
            {
                usleep(100 * 1000);
                break;
            }

            printf("==========\nMessage Received\n");
            printf("<%s>\n",rxBuffer);
            printf("==========\nEnd of Message Received\n");

            if (readSize && rtspRx((RtspClientRx*)&client, rxBuffer, readSize))
            {
                fprintf(stderr, "rtspRx() returned error.\n");
                break;
            }
        }

        close(client.socket);
        #if 0
        break;
        #endif
    }

    close(serverSock);
    return NULL;
}

int rtspServerStart(pthread_t *rtspServerThread)
{
    int rtn = startRtsp();

    if (rtn)
    {
        return rtn;
    }

    rtn = pthread_create(rtspServerThread, NULL, rtspServer, NULL);

    return rtn;
}


int main(void)
{
    int rtn = 0;
    pthread_t rtspServerThread;
    pthread_t rtpServerThread;
    void *status = NULL;

    if (rtn = rtspServerStart(&rtspServerThread))
    {
        return rtn;
    }

#if 0
    rtn = rtpServerStart(&rtpServerThread);
    if (rtn) return rtn;
#endif

    pthread_join(rtspServerThread, &status);
#if 0
    pthread_join(rtpServerThread);
#endif

    return rtn;
}
