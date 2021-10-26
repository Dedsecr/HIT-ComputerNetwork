#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include "server.h"
#include <iostream>

#include "debug.cpp"

#pragma comment(lib, "ws2_32.lib")

Server::Server(int port)
{
    // Init WSA
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        printf("Init WSA Error!\n");
        return;
    }

    // Create a new socket
    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (slisten == INVALID_SOCKET)
    {
        printf("Create a new socket Error!\n");
        return;
    }

    // Init listening
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        printf("Init listening Error!\n");
        return;
    }

    this->slisten = slisten;
}

void Server::start()
{
    // Start listening
    if (listen(this->slisten, 5) == SOCKET_ERROR)
    {
        printf("Start listening Error!\n");
        return;
    }

    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    while (true)
    {
        printf("Waiting for connection...\n");
        if ((sClient = accept(this->slisten, (SOCKADDR *)&remoteAddr, &nAddrlen)) == INVALID_SOCKET)
        {
            printf("Accept Error!\n");
            continue;
        }
        printf("Recieve a connection from %s:%d\n", inet_ntoa(remoteAddr.sin_addr), remoteAddr.sin_port);
        CreateThreadToHandleRequest(sClient);
    }
    closesocket(this->slisten);
    WSACleanup();
}

ERROR_CODE Server::ConnectToServer(SOCKET *serverSocket, char *host)
{
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);

    ERROR_CODE siteFilterResult = SiteFilter(host);
    if (siteFilterResult == CHANGE_SUCCEEDED)
        printf("Successfully redirected host to %s\n", host);
    else if (siteFilterResult == HOST_BLOCKED)
        return HOST_BLOCKED;

    HOSTENT *hostent = gethostbyname(host);
    if (!hostent)
        return REQUEST_FAILED;

    in_addr Inaddr = *((in_addr *)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverSocket == INVALID_SOCKET)
        return REQUEST_FAILED;

    if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        closesocket(*serverSocket);
        return REQUEST_FAILED;
    }
    return REQUEST_SUCCEEDED;
}

DWORD Server::RequestFinished(SOCKET socket, SOCKET serverSocket, ERROR_CODE ErrorCode)
{
    if (ErrorCode != REQUEST_SUCCEEDED)
        printf("## An error occurred: %d\n", ErrorCode);
    printf("Closing socket...\n");
    Sleep(200);
    closesocket(socket);
    closesocket(serverSocket);
    return 1;
}

DWORD WINAPI Server::RequestThread(LPVOID args)
{
    SOCKET socket = *(SOCKET *)args, serverSocket;
    char Buffer[BUFFER_MAXSIZE];
    ZeroMemory(Buffer, BUFFER_MAXSIZE);

    int recvLength = recv(socket, Buffer, BUFFER_MAXSIZE, 0);

    HttpHeaderP httpHeader = ParseHttpHeader(Buffer, recvLength);

    ERROR_CODE ec = ConnectToServer(&serverSocket, httpHeader->host);
    if (ec != REQUEST_SUCCEEDED)
        return RequestFinished(socket, serverSocket, ec);
    printf("Successfully connected host %s %d\n", httpHeader->host, Cache_storage_length);
    // puts("!!!!!!!!!!!!!!");
    WebCacheP cacheP = WebCache::FindCache(httpHeader);

    // puts("!!!!!!!!!!!!!!");
    if (cacheP)
        return RequestFinished(socket, serverSocket, RequestUsingCache(cacheP, socket, serverSocket, Buffer, recvLength, httpHeader));
    // puts("!!!!!!!!!!!!!!");
    send(serverSocket, Buffer, recvLength, 0);

    recvLength = recv(serverSocket, Buffer, BUFFER_MAXSIZE, 0);
    if (recvLength <= 0)
        return RequestFinished(socket, serverSocket, NO_RETURN_INFO);
    send(socket, Buffer, recvLength, 0);
    return RequestFinished(socket, serverSocket, UpdateCache(cacheP, httpHeader, Buffer, recvLength));
}

ERROR_CODE Server::UpdateCache(WebCacheP cacheP, HttpHeaderP httpHeader, char *Buffer, int recvLength)
{
    char *cacheBuffer = new char[BUFFER_MAXSIZE];
    ZeroMemory(cacheBuffer, BUFFER_MAXSIZE);
    memcpy(cacheBuffer, Buffer, BUFFER_MAXSIZE);
    // print_debug_buffer(Buffer);
    // print_debug_htp(httpHeader);
    const char *delim = "\r\n";
    char *ptr;
    char date[DATE_LENGTH];
    ZeroMemory(date, sizeof(date));
    char *p = strtok_s(cacheBuffer, delim, &ptr);
    bool isUpdate = false;
    while (p)
    {
        if (p[0] == 'L')
        {
            if (strlen(p) > 15)
            {
                char header[15];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 14);
                if (!(strcmp(header, "Last-Modified:")))
                {
                    memcpy(date, &p[15], strlen(p) - 15);
                    isUpdate = true;
                    break;
                }
            }
        }
        p = strtok_s(NULL, delim, &ptr);
    }
    //如果有更新，将新的报文放到缓存里
    if (isUpdate)
    {
        if (cacheP == NULL)
        {
            cacheP = Cache_storage[Cache_storage_length % CACHE_MAXSIZE] = new WebCache();
            Cache_storage_length++;
            memcpy(cacheP->htP->method, httpHeader->method, strlen(httpHeader->method));
            memcpy(cacheP->htP->url, httpHeader->url, strlen(httpHeader->url));
            memcpy(cacheP->htP->host, httpHeader->host, strlen(httpHeader->host));
        }
        memcpy(cacheP->buffer, Buffer, recvLength);
        memcpy(cacheP->date, date, strlen(date));
        // print_debug_htp(cacheP->htP);
    }
    return REQUEST_SUCCEEDED;
}

ERROR_CODE Server::RequestUsingCache(WebCacheP cacheP, SOCKET socket, SOCKET serverSocket, char *Buffer, int recvLength, HttpHeaderP httpHeader)
{
    // puts("@@@@@@@@@@@@@");
    char *CacheBuffer;
    int i = 0, length = 0, length2 = 0;
    length = recvLength;
    char const *ife = "If-Modified-Since: ";
    strcat(Buffer, ife);
    strcat(Buffer, cacheP->date);
    length = length + strlen(ife) + strlen(cacheP->date);

    Buffer[length++] = '\r';
    Buffer[length++] = '\n';

    send(serverSocket, Buffer, length, 0);

    //等待目标服务器返回数据
    recvLength = recv(serverSocket, Buffer, BUFFER_MAXSIZE, 0);
    if (recvLength <= 0)
        return NO_RETURN_INFO;

    const char *Modd = "304";

    // puts(&Buffer[9]);
    // puts("@@@@@@@@@@@@@");

    if (!memcmp(&Buffer[9], Modd, strlen(Modd)))
    {
        printf("Using cache\n");
        send(socket, cacheP->buffer, cacheP->buffer_length, 0);
    }
    else
        send(serverSocket, Buffer, recvLength, 0);

    return UpdateCache(cacheP, httpHeader, Buffer, recvLength);
}

HttpHeaderP Server::ParseHttpHeader(char *Buffer, int recvLength)
{
    char *CacheBuffer = new char[recvLength + 1];
    ZeroMemory(CacheBuffer, recvLength + 1);
    memcpy(CacheBuffer, Buffer, recvLength);
    Buffer = CacheBuffer;
    HttpHeaderP httpHeader = new HttpHeader();
    char *p, *ptr;
    const char *delim = "\r\n";
    p = strtok_s(Buffer, delim, &ptr);
    printf("%s\n", p);
    if (p[0] == 'G')
    { //GET 方式
        memcpy(httpHeader->method, "GET", 3);
        memcpy(httpHeader->url, &p[4], strlen(p) - 13);
    }
    else if (p[0] == 'P')
    { //POST 方式
        memcpy(httpHeader->method, "POST", 4);
        memcpy(httpHeader->url, &p[5], strlen(p) - 14);
    }
    printf("%s\n", httpHeader->url);
    p = strtok_s(NULL, delim, &ptr);
    while (p)
    {
        switch (p[0])
        {
        case 'H': //Host
            memcpy(httpHeader->host, &p[6], strlen(p) - 6);
            break;
        case 'C': //Cookie
            if (strlen(p) > 8)
            {
                char header[8];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 6);
                if (!strcmp(header, "Cookie"))
                {
                    memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
                }
            }
            break;
        default:
            break;
        }
        p = strtok_s(NULL, delim, &ptr);
    }
    return httpHeader;
}

void Server::CreateThreadToHandleRequest(SOCKET socket)
{
    DWORD threadID;
    HANDLE threadHandle = CreateThread(NULL, 0, RequestThread, &socket, 0, &threadID);
    CloseHandle(threadHandle);
    Sleep(200);
}

WebCacheP WebCache::FindCache(HttpHeaderP htp)
{
    // puts("*************************************************");
    for (int i = 0; i < CACHE_MAXSIZE; i++)
    {
        if (Cache_storage[i] == NULL)
            continue;
        HttpHeaderP htp1 = Cache_storage[i]->htP, htp2 = htp;
        if (!strcmp(htp1->method, htp2->method) && !strcmp(htp1->url, htp2->url) && !strcmp(htp1->host, htp2->host))
            return Cache_storage[i];
        // print_debug_htp(htp1);
        // print_debug_htp(htp2);
    }
    // puts("*************************************************");
    return NULL;
}

ERROR_CODE SiteFilter(char *host)
{
    for (int i = 0; i < DISABLED_MAXSIZE; i++)
    {
        if (disabledHost[i] == NULL)
            continue;
        if (strcmp(disabledHost[i], host) == 0)
            return HOST_BLOCKED;
    }
    for (int i = 0; i < DISABLED_MAXSIZE; i++)
    {
        if (induceSites[i] == NULL)
            continue;
        if (strcmp(induceSites[i], host) == 0)
        {
            strcpy(host, targetSites[i]);
            return CHANGE_SUCCEEDED;
        }
    }
    return CHANGE_FAILED;
}