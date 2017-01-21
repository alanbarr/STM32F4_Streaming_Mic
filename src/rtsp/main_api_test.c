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
#include "rtsp.h"
#include "rtsp_internal.h"

#define ASSERT(X)   \
    if (!(X)) \
    {\
        fprintf(stderr, "%s %d ASSERT FAILED!\n", __FILE__, __LINE__);\
        exit(__LINE__);\
    }\
    else\
    {\
        fprintf(stderr, "%s %d ASSERT PASSED\n", __FILE__, __LINE__);\
    }


static RtspStatus sendData(RtspClientRx *client,
                           const uint8_t *buffer,
                           uint16_t size);

static RtspStatus control(RtspSession *session,
                          RtspSessionInstruction instruction);

extern RtspRequest lastRequest;

struct {
  RtspSession session;
  RtspSessionInstruction instruction;
} lastControlCb;


static const char * const sdp = 
        "v=0\r\n"
        "o=derp 1 1 IN IPV4 192.168.1.178\r\n"
        "s=derp\r\n"
        "t=0 0\r\n"
        "m=audio 49170 RTP/AVP 98\r\n"
        "a=rtpmap:98 L16/16000/18\r\n";

static int currentClientSock = 0;

int stringTests(void)
{
    char * string = NULL;
    RtspConfig config;
    RtspResource resource;
    RtspResource *resourceHandle;
    RtspClientRx client;
    int dummyHandle = 43;

    memset(&config, 0, sizeof(config));
    config.callback.tx = sendData;
    config.callback.control = control;

    ASSERT(RTSP_OK == rtspInit(&config));

    memset(&resource, 0, sizeof(resource));
    string = "/derp";
    strcpy(resource.path.data, string);
    resource.path.length = strlen(string);
    resource.sdp.data    = sdp;
    resource.sdp.length  = strlen(sdp);

    resource.sdp.data    = sdp;
    resource.sdp.length  = strlen(sdp);
    ASSERT(RTSP_OK == rtspResourceInit(&resource, &resourceHandle));

/******************************************************************************/
    string = "SETUP rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 100\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Transport: RTP/AVP;unicast;client_port=49170-49171\r\n"
             "\r\n";


    memset(&client, 0, sizeof(client));
    client.handle.userDefined = &dummyHandle;

    memset(&lastRequest, 0, sizeof(lastRequest));
    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastRequest.method == RTSP_METHOD_SETUP);
    ASSERT(lastRequest.header.cseq.length == 9);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 100", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);

    ASSERT(lastRequest.header.transport.rtp == 1);
    ASSERT(lastRequest.header.transport.unicast == 1);
    ASSERT(lastRequest.header.transport.rtpData.clientPortRange[0] == 49170);
    ASSERT(lastRequest.header.transport.rtpData.clientPortRange[1] == 49171);

    ASSERT(lastControlCb.instruction == RTSP_SESSION_CREATE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);

/******************************************************************************/
    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Session: DEADBEEF\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    memset(&lastControlCb, 0x0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    string = "/derp";
    ASSERT(lastRequest.path.length == strlen(string));
    ASSERT(strncmp(lastRequest.path.buffer, string, strlen(string)) ==0);

    /* should remain memset to 0 */
    ASSERT(lastControlCb.instruction == 0);
    ASSERT(lastControlCb.session.resource == 0);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == 0);
    ASSERT(lastControlCb.session.client.portRange[0] == 0);
    ASSERT(lastControlCb.session.client.portRange[1] == 0);


/******************************************************************************/
    string = "DESCRIBE rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 3\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Accept: application/sdp\r\n"
             "Session: DEADBEEF\r\n"
             "\r\n";

    memset(&lastControlCb, 0x0, sizeof(lastControlCb));
    memset(&lastRequest,0,sizeof(lastRequest));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastRequest.method == RTSP_METHOD_DESCRIBE);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 3", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 1);

    /* Force a teardown so next setup works */
    string = "TEARDOWN rtsp://192.168.1.180:13001/derp RTSP/1.0\r\n"
              "CSeq: 4\r\n"
              "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
              "Session: DEADBEEF\r\n"
              "\r\n";
    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_DELETE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);
/******************************************************************************/

    string = "SETUP rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 100\r\n"
             "Transport: RTP/AVP;multicast;ttl=127;mode=\"PLAY\","
             "RTP/AVP;unicast;client_port=3456-3457;mode=\"PLAY\"\r\n"
             "\r\n";

    memset(&lastRequest, 0, sizeof(lastRequest));
    memset(&lastControlCb, 0x0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastRequest.header.transport.rtpData.clientPortRange[0] == 3456);
    ASSERT(lastRequest.header.transport.rtpData.clientPortRange[1] == 3457);

    ASSERT(lastControlCb.instruction == RTSP_SESSION_CREATE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 3456);
    ASSERT(lastControlCb.session.client.portRange[1] == 3457);

/******************************************************************************/
    /* Testing connection header */
    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: close\r\n"
             "Session: DEADBEEF\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    ASSERT(lastRequest.header.connection == RTSP_CONNECTION_CLOSE);

    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: Keep-Alive\r\n"
             "Session: DEADBEEF\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    ASSERT(lastRequest.header.connection == RTSP_CONNECTION_KEEPALIVE);

/******************************************************************************/
    /* Testing session header */
    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: Keep-Alive\r\n"
             "Session: 12345\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    fprintf(stderr, "length is %u\n", lastRequest.header.session.length);
    ASSERT(lastRequest.header.session.length == 5);
    ASSERT(strncmp("12345",
                   lastRequest.header.session.data,
                   lastRequest.header.session.length) == 0);

    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: Keep-Alive\r\n"
             "Session: 12345;timeout=5\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    fprintf(stderr, "length is %u\n", lastRequest.header.session.length);
    ASSERT(lastRequest.header.session.length == 5);
    ASSERT(strncmp("12345",
                   lastRequest.header.session.data,
                   lastRequest.header.session.length) == 0);

/******************************************************************************/
    /* Require */
    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: Keep-Alive\r\n"
             "Session: 12345;timeout=5\r\n"
             "Require: derp-derp-derp-derp\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    ASSERT(lastRequest.header.session.length == 5);
    ASSERT(strncmp("12345",
                   lastRequest.header.session.data,
                   lastRequest.header.session.length) == 0);

    string = "derp-derp-derp-derp";
    ASSERT(lastRequest.header.require.count == 1);
    ASSERT(lastRequest.header.require.unsupported[0].length ==
            strlen(string));
    ASSERT(strncmp(string,
                   lastRequest.header.require.unsupported[0].buffer,
                   lastRequest.header.require.unsupported[0].length) == 0);


    string = "OPTIONS rtsp://192.168.1.178:13001/derp RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Connection: Keep-Alive\r\n"
             "Session: DEADBEEF;timeout=5\r\n"
             "Require: derp\r\n"
             "Require: derp12345\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));
    ASSERT(lastRequest.method == RTSP_METHOD_OPTIONS);
    ASSERT(lastRequest.header.cseq.length == 7);
    ASSERT(strncmp(lastRequest.header.cseq.buffer, "CSeq: 2", 7) == 0);
    ASSERT(lastRequest.header.accept.appSdp == 0);
    ASSERT(lastRequest.header.session.length == 8);
    ASSERT(strncmp("DEADBEEF",
                   lastRequest.header.session.data,
                   lastRequest.header.session.length) == 0);

    ASSERT(lastRequest.header.require.count == 2);
    string = "derp";
    ASSERT(lastRequest.header.require.unsupported[0].length == strlen(string));
    ASSERT(strncmp(string,
                   lastRequest.header.require.unsupported[0].buffer,
                   lastRequest.header.require.unsupported[0].length) == 0);
    string = "derp12345";
    ASSERT(lastRequest.header.require.unsupported[1].length == strlen(string));
    ASSERT(strncmp(string,
                   lastRequest.header.require.unsupported[1].buffer,
                   lastRequest.header.require.unsupported[1].length) == 0)



/******************************************************************************/
    /* Force a teardown so next setup works */
    string = "TEARDOWN rtsp://192.168.1.180:13001/derp RTSP/1.0\r\n"
              "CSeq: 4\r\n"
              "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
              "Session: DEADBEEF\r\n"
              "\r\n";

    memset(&lastControlCb, 0, sizeof(lastControlCb));
    memset(&lastRequest,0,sizeof(lastRequest));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_DELETE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 3456);
    ASSERT(lastControlCb.session.client.portRange[1] == 3457);

/******************************************************************************/

    /* VLC Examples */
    string = "SETUP rtsp://192.168.1.180:13001/derp RTSP/1.0\r\n"
             "CSeq: 4\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Transport: RTP/AVP;unicast;client_port=49170-49171\r\n"
             "\r\n";

    memset(&lastControlCb, 0, sizeof(lastControlCb));
    memset(&lastRequest,0,sizeof(lastRequest));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_CREATE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);


/******************************************************************************/
    /* Force a teardown so next setup works */
    string = "TEARDOWN rtsp://192.168.1.180:13001/derp RTSP/1.0\r\n"
              "CSeq: 4\r\n"
              "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
              "Session: DEADBEEF\r\n"
              "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_DELETE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);

/******************************************************************************/

    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspResourceShutdown(resourceHandle));
/******************************************************************************/
    /* Testing URI / Path */
    memset(&resource, 0, sizeof(resource));
    string = "/";
    strcpy(resource.path.data, string);
    resource.path.length = strlen(string);


    resource.sdp.data    = sdp;
    resource.sdp.length  = strlen(sdp);
    ASSERT(RTSP_OK == rtspResourceInit(&resource, &resourceHandle));

    string = "SETUP rtsp://192.168.1.178:13001/ RTSP/1.0\r\n"
             "CSeq: 100\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Transport: RTP/AVP;unicast;client_port=49170-49171\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    string = "/";
    ASSERT(lastRequest.path.length == strlen(string));
    ASSERT(strncmp(lastRequest.path.buffer, string, strlen(string)) ==0);

    ASSERT(lastControlCb.instruction == RTSP_SESSION_CREATE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);



/******************************************************************************/
    /* Force a teardown so next setup works */
    string = "TEARDOWN rtsp://192.168.1.180:13001/ RTSP/1.0\r\n"
              "CSeq: 4\r\n"
              "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
              "Session: DEADBEEF\r\n"
              "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));
    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_DELETE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);

/******************************************************************************/

    ASSERT(RTSP_OK == rtspResourceShutdown(resourceHandle));
/******************************************************************************/

    memset(&resource, 0, sizeof(resource));
    string = "";
    strcpy(resource.path.data, string);
    resource.path.length = strlen(string);

    string = "SETUP rtsp://192.168.1.178:13001 RTSP/1.0\r\n"
             "CSeq: 100\r\n"
             "User-Agent: LibVLC/2.2.4 (LIVE555 Streaming Media v2016.02.22)\r\n"
             "Transport: RTP/AVP;unicast;client_port=49170-49171\r\n"
             "\r\n";

    memset(&lastRequest,0,sizeof(lastRequest));

    resource.sdp.data    = sdp;
    resource.sdp.length  = strlen(sdp);
    ASSERT(RTSP_OK == rtspResourceInit(&resource, &resourceHandle));

    memset(&lastRequest,0,sizeof(lastRequest));
    memset(&lastControlCb, 0, sizeof(lastControlCb));

    ASSERT(RTSP_OK == rtspRx(&client, string, strlen(string)));

    string = "";
    ASSERT(lastRequest.path.length == strlen(string));
    ASSERT(strncmp(lastRequest.path.buffer, string, strlen(string)) ==0);

    ASSERT(lastControlCb.instruction == RTSP_SESSION_CREATE);
    ASSERT(lastControlCb.session.resource == resourceHandle);
    ASSERT(lastControlCb.session.client.info.handle.userDefined == &dummyHandle);
    ASSERT(lastControlCb.session.client.portRange[0] == 49170);
    ASSERT(lastControlCb.session.client.portRange[1] == 49171);


    ASSERT(RTSP_OK == rtspResourceShutdown(resourceHandle));

    ASSERT(lastControlCb.instruction == RTSP_SESSION_DELETE);

    printf("\n=======\nSUCCESS\n=======\n");
    return 0;
}

RtspStatus sendData(RtspClientRx *client,
                    const uint8_t *buffer,
                    uint16_t size)
{
    int rtn = 0;

    printf("==========\n Transmit buffer:\n %s", buffer);
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

    lastControlCb.session     = *session;
    lastControlCb.instruction = instruction;
    return RTSP_OK;
}

int server(void)
{
    int serverSock;
    int clientSock;
    int c;
    int readSize;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    char rxBuffer[2000];
    const int port = 13001;
    RtspClientRx client;

    printf("Creating socket.\n");
    serverSock = socket(AF_INET, SOCK_STREAM, 0);

    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_addr.s_addr  = INADDR_ANY;
    serverAddr.sin_port         = htons(port);

    printf("Binding socket.\n");
    if (bind(serverSock,
             (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) < 0)
    {
        perror("bind failed. Error");
        return 1;
    }

    printf("Listening.\n");
    if (listen(serverSock, 1))
    {
        perror("listen failed.");
        return 1;
    }

    printf("Waiting for incoming connections on port %u\n", port);
    c = sizeof(struct sockaddr_in);

    while (1)
    {
        clientSock = accept(serverSock,
                            (struct sockaddr *)&clientAddr,
                            (socklen_t*)&c);

        if (clientSock < 0)
        {
            perror("accept failed");
            return 1;
        }

        printf("Connection accepted\n");

        currentClientSock = clientSock;

        while (1)
        {
            readSize = recv(clientSock, rxBuffer, 2000, 0);

            if (readSize < 0)
            {
                fprintf(stderr, "recv() returned error.\n");
                break;
            }

            if (readSize == 0)
            {
                fprintf(stderr, "recv() returned 0\n");
                break;
            }

            printf("==========\nMessage Received\n");
            printf("<%s>\n",rxBuffer);
            if (readSize && rtspRx(&client, rxBuffer, readSize))
            {
                fprintf(stderr, "rtspRx() returned error.\n");
                break;
            }
        }

        close(clientSock);
        break;
    }

    close(serverSock);
    return 0;
}

int main(void)
{
    return stringTests();
}
