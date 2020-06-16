#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 
#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 100
#define MAX_CLNT 256

unsigned WINAPI HandleClnt(void* arg);
void SendMsg(SOCKET sock, char* msg, int len);
void ErrorHandling(const char* msg);

enum States {
	NONE,
	WaitingRequest,
	WaitingAnswer,
	Connected
};

typedef struct sock_state {
	char name[100];
	enum States state;
	int connectwith;
}SOCKSTATE;

map<SOCKET, SOCKSTATE> clntSocks;
vector<pair<SOCKET, SOCKET> > requests;
vector<pair<SOCKET, SOCKET> >chatrooms;

HANDLE hMutex, hEvent;

void HandleData(char* msg, int msgCount, SOCKET sock);
SOCKET getSocketFromName(char* name);

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;
	SOCKSTATE state;
	int clntAdrSz;
	char* nameBuf;
	char buf[BUF_SIZE];
	int strLen;
	int fdNum;
	HANDLE  hThread, hNamingThread;

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

		hThread =
			(HANDLE)_beginthreadex(NULL, 0, HandleClnt, (void*)&hClntSock, 0, NULL);
		printf("Connected client IP: %s \n", inet_ntoa(clntAdr.sin_addr));
	}
	closesocket(hServSock);
	WSACleanup();
	return 0;
}

SOCKET getSocketFromName(char* name) {
	WaitForSingleObject(hMutex, INFINITE);
	SOCKET ret = SOCKET_ERROR;
	for(auto it = clntSocks.begin(); it != clntSocks.end(); it++)
		if (strcmp(name, it->second.name) == 0) {
			ret = it->first;
			break;
		}
	ReleaseMutex(hMutex);
	return ret;
}

void HandleData(char* msg, int msgCount, SOCKET sock) {
	char buf[BUF_SIZE];
	char name[BUF_SIZE];
	int recvSize;
	auto clntItem = clntSocks.find(sock);
	SOCKET temp;
	
	if (strcmp(msg, "showlist") == 0) {		// 대화중인 클라이언트 따로 빼기
		buf[0] = (char)1;
		send(sock, buf, sizeof(char), 0);
		send(sock, buf, sizeof(char), 0);
		buf[0] = (char)clntSocks.size();
		send(sock, buf, sizeof(char), 0);
		for (auto it = clntSocks.begin(); it != clntSocks.end(); it++) {
			sprintf_s(buf, "%s\n", (*it).second.name);
			send(sock, buf, strlen(buf), 0);
		}
	}
	else if (strncmp(msg, "requestchat", 11) == 0) {
		if (msgCount == 12) {
			recvSize = recv(sock, buf, BUF_SIZE, 0);
			buf[recvSize] = '\0';
			sprintf_s(name, "[%s]", buf);
		}
		else {
			sprintf_s(name, "[%s]", msg + 12);
		}

		if ((temp = getSocketFromName(name)) != SOCKET_ERROR) {
			buf[0] = 2;
			send(temp, buf, 1, 0);
			send(temp, clntSocks.find(sock)->second.name, strlen(name), 0);
			clntSocks.find(temp)->second.state = WaitingAnswer;
			clntSocks.find(sock)->second.state = WaitingRequest;
			requests.push_back(make_pair(temp, sock));
		}
		else {
			send(sock, "2", 1, 0);
			send(sock, "I", 1, 0);
		}
	}
}

void breakroom(SOCKET sock) {
	char msg[] = "q";

	for (auto it = chatrooms.begin(); it != chatrooms.end(); it++) {
		if (it->first == sock) {
			send(it->second, msg, 1, 0);
			clntSocks.find(it->second)->second.state = NONE;
			chatrooms.erase(it);
			break;
		}
		else if (it->second == sock) {
			send(it->first, msg, 1, 0);
			clntSocks.find(it->first)->second.state = NONE;
			chatrooms.erase(it);
			break;
		}
	}
	clntSocks.find(sock)->second.state = NONE;
}

int setSocketState(SOCKET s, enum States state) {
	auto it = clntSocks.find(s);
	if (it != clntSocks.end()) {
		it->second.state = state;
		return 0;
	}
	else {
		return 1;
	}
}

unsigned WINAPI HandleClnt(void* arg)
{
	SOCKET hClntSock = *((SOCKET*)arg);
	enum States s;
	SOCKSTATE state;

	int strLen = 0, i;
	char name[100];
	char msg[BUF_SIZE];
	char msgToSend[BUF_SIZE + 100];

	while (1) {
		strLen = recv(hClntSock, name, 100, 0);
		if (strLen <= 0) {
			closesocket(hClntSock);
			return 0;
		}
		name[strLen] = '\0';
		if (getSocketFromName(name) == SOCKET_ERROR) {
			send(hClntSock, "Y", 1, 0);
			break;
		}
		else {
			send(hClntSock, "N", 1, 0);
		}
	}

	strcpy_s(state.name, name);
	state.state = NONE;

	clntSocks.insert(make_pair(hClntSock, state));

	while (1) {
		strLen = recv(hClntSock, msg, sizeof(msg), 0);
		if (strLen < 0) break;
		msg[strLen] = '\0';
		s = clntSocks.find(hClntSock)->second.state;
		if(s == NONE) HandleData(msg, strLen, hClntSock);

		else if (s == WaitingAnswer) {
			for (auto it = requests.begin(); it != requests.end(); it++) {
				if (it->first == hClntSock) {
					if (msg[0] == 'Y') {
						if (setSocketState(it->second, Connected)) {
							send(hClntSock, "N", 1, 0);
							requests.erase(it);
							setSocketState(hClntSock, NONE);
							break;
						}
						setSocketState(hClntSock, Connected);
						chatrooms.push_back(*it);
					}
					else {
						setSocketState(hClntSock, NONE);
						setSocketState(it->second, NONE);
					}
					send(hClntSock, "O", 1, 0);
					send(it->second, msg, 1, 0);
					requests.erase(it);
					break;
				}
			}
		}

		else if (s == Connected) {
			if (strLen == 1 && (msg[0] == 'q' || msg[0] == 'Q')) {
				breakroom(hClntSock);
			}
			sprintf_s(msgToSend, "%s %s\n", name, msg);
			SendMsg(hClntSock, msgToSend, strlen(msgToSend));
		}
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

void SendMsg(SOCKET sock, char* msg, int len)
{
	int i;
	WaitForSingleObject(hMutex, INFINITE);
	for (auto it = chatrooms.begin(); it != chatrooms.end(); it++) {
		if (it->first == sock) {
			send(it->second, msg, 1, 0);
			send(it->second, msg, len, 0);
			break;
		}
		else if (it->second == sock) {
			send(it->first, msg, 1, 0);
			send(it->first, msg, len, 0);
			break;
		}
	}

	ReleaseMutex(hMutex);
}
void ErrorHandling(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}