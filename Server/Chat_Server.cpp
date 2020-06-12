#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 
#include <vector>
#include <string>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 100
#define MAX_CLNT 256

unsigned WINAPI HandleClnt(void* arg);
void SendMsg(char* msg, int len);
void ErrorHandling(const char* msg);

vector<pair<SOCKET, char*> > clntSocks;
vector<pair<SOCKET, SOCKET> >chatrooms;

HANDLE hMutex;

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;
	int clntAdrSz;
	char* nameBuf;
	int nameSize;
	HANDLE  hThread;

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	hMutex = CreateMutex(NULL, FALSE, NULL);
	hServSock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	while (1)
	{
		clntAdrSz = sizeof(clntAdr);
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &clntAdrSz);

		WaitForSingleObject(hMutex, INFINITE);
		nameBuf = (char*)malloc(sizeof(char) * 100);
		nameSize = recv(hClntSock, nameBuf, 100, 0);
		nameBuf[nameSize] = '\0';

		clntSocks.push_back(make_pair(hClntSock, nameBuf));
		ReleaseMutex(hMutex);

		hThread =
			(HANDLE)_beginthreadex(NULL, 0, HandleClnt, (void*)&hClntSock, 0, NULL);
		printf("Connected client IP: %s \n", inet_ntoa(clntAdr.sin_addr));
	}
	closesocket(hServSock);
	WSACleanup();
	return 0;
}

unsigned WINAPI HandleClnt(void* arg)
{
	SOCKET hClntSock = *((SOCKET*)arg);
	int strLen = 0, i;
	char* name = NULL;
	char msg[BUF_SIZE];
	char msgToSend[BUF_SIZE + 100];

	for (auto it = clntSocks.begin(); it != clntSocks.end(); it++) {
		if (hClntSock == (*it).first) {
			name = (*it).second;
			break;
		}
	}

	while (1) {
		strLen = recv(hClntSock, msg, sizeof(msg), 0);
		if (strLen < 0) break;
		msg[strLen] = '\0';
		if (strcmp(msg, "showlist") == 0) {
			printf("send!\n");
			for (auto it = clntSocks.begin(); it != clntSocks.end(); it++) {
				printf("len %d\n", strlen((*it).second));
				send(hClntSock, (*it).second, strlen((*it).second), 0);
			}
			send(hClntSock, "/end", 5, 0);
		}
		else if (strcmp(msg, "request") == 0) {

		}
	}

	while ((strLen = recv(hClntSock, msg, sizeof(msg), 0)) > 0) {
		msgToSend[0] = '\0';
		msg[strLen] = '\0';
		strcat_s(msgToSend, name);
		strcat_s(msgToSend, " ");
		strcat_s(msgToSend, msg);

		SendMsg(msgToSend, strlen(msgToSend));
	}

	WaitForSingleObject(hMutex, INFINITE);
	for (auto it = clntSocks.begin(); it != clntSocks.end(); it++)   // remove disconnected client
	{
		if (hClntSock == (*it).first) {
			clntSocks.erase(it);
			break;
		}
	}
	ReleaseMutex(hMutex);
	closesocket(hClntSock);
	return 0;
}

void SendMsg(char* msg, int len)   // send to all
{
	int i;
	WaitForSingleObject(hMutex, INFINITE);
	for (i = 0; i < clntSocks.size(); i++)
		send(clntSocks[i].first, msg, len, 0);

	ReleaseMutex(hMutex);
}
void ErrorHandling(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}