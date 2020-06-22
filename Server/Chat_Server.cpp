#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <windows.h>
#include <process.h> 
#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1000
#define MAX_CLNT 256

void HandleCommand(char* msg, int msgCount, SOCKET sock);
SOCKET getSocketFromName(char* name);
void breakroom(SOCKET sock);
int group_exit(SOCKET sock);
int setSocketState(SOCKET s, enum States state);
unsigned WINAPI HandleClnt(void* arg);
void SendMsg(SOCKET sock, char* msg, int len);
int isGroupCommand(SOCKET sock, char* msg);
void SendGroupMsg(SOCKET sock, char* msg, int len);
void sendMsgWithProtocol(SOCKET sock, char* msg);
void ErrorHandling(const char* msg);

enum States {
	NONE,
	WaitingRequest,
	WaitingAnswer,
	Connected,
	GroupConnected,
	GroupAnnounce
};

typedef struct sock_state {
	char name[100];
	enum States state;
	int connectwith;
}SOCKSTATE;

map<SOCKET, SOCKSTATE> clntSocks;
vector<pair<SOCKET, SOCKET> > requests;
vector<pair<SOCKET, SOCKET> >chatrooms;
map<int, vector<SOCKET> > group_chats;
map<int, vector<char*> > group_announce;

HANDLE hMutex, hEvent;

int waitingroomCnt = 0;

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

unsigned WINAPI HandleClnt(void* arg)
{
	SOCKET hClntSock = *((SOCKET*)arg);
	enum States s;
	SOCKSTATE state;

	int strLen = 0, i;
	char name[100];
	char msg[BUF_SIZE];
	char msgToSend[BUF_SIZE + 100];

	srand((unsigned int)time(NULL));

	while (1) {											// Set Name
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
		s = clntSocks.find(hClntSock)->second.state;
		if (s == WaitingAnswer && strLen < 0) {
			for (auto it = requests.begin(); it != requests.end(); it++) {
				if (it->first == hClntSock) {
					setSocketState(it->second, NONE);
					
					send(it->second, "T", 1, 0);
					requests.erase(it);
					break;
				}
			}
		}

		if (strLen < 0) break;
		msg[strLen] = '\0';

		if(s == NONE) HandleCommand(msg, strLen, hClntSock);

		else if (s == WaitingAnswer) {
			for (auto it = requests.begin(); it != requests.end(); it++) {
				if (it->first == hClntSock) {
					if (msg[0] == 'Y') {
						if (setSocketState(it->second, Connected)) {
							send(hClntSock, "N", 1, 0);						// Connect Error From another client
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
			if (strncmp(msg, "!!\n//quit//!!", 13) == 0) {
				breakroom(hClntSock);
			}
			sprintf_s(msgToSend, "%s %s\n", name, msg);
			SendMsg(hClntSock, msgToSend, strlen(msgToSend));
		}

		else if (s == GroupConnected) {
			if (!isGroupCommand(hClntSock, msg)) {
				if (strncmp(msg, "!!\n//quit//!!", 13) == 0) {
					if (group_exit(hClntSock)) continue;
					else {
						strcpy_s(msg, "님이 나가셨습니다.\n");
					}
				}
				sprintf_s(msgToSend, "%s %s\n", name, msg);
				SendGroupMsg(hClntSock, msgToSend, strlen(msgToSend));
			}
		}
		else if (s == GroupAnnounce) {
			char* announceMsg;
			auto& announce_v = group_announce.find(clntSocks.find(hClntSock)->second.connectwith)->second;
			announceMsg = (char*)malloc(sizeof(char) * strlen(msg) + 1);
			strcpy_s(announceMsg, strlen(msg) + 1, msg);
			announce_v.push_back(announceMsg);

			strcpy_s(msg, BUF_SIZE, "님이 공지사항을 등록했습니다.\n");
			setSocketState(hClntSock, GroupConnected);
			sprintf_s(msgToSend, "%s %s\n", name, msg);
			SendGroupMsg(hClntSock, msgToSend, strlen(msgToSend));
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

SOCKET getSocketFromName(char* name) {
	WaitForSingleObject(hMutex, INFINITE);
	SOCKET ret = SOCKET_ERROR;
	for (auto it = clntSocks.begin(); it != clntSocks.end(); it++)
		if (strcmp(name, it->second.name) == 0) {
			ret = it->first;
			break;
		}
	ReleaseMutex(hMutex);
	return ret;
}

void HandleCommand(char* msg, int msgCount, SOCKET sock) {
	char buf[BUF_SIZE];
	char name[BUF_SIZE];
	int recvSize;
	int num;
	auto clntItem = clntSocks.find(sock);
	SOCKET temp;

	if (strcmp(msg, "showlist") == 0) {
		buf[0] = (char)1;
		send(sock, buf, 1, 0);
		for (auto it = clntSocks.begin(); it != clntSocks.end(); it++) {
			if (it->second.state == NONE) {
				sprintf_s(buf, "%s\n", (*it).second.name);
				send(sock, buf, strlen(buf), 0);
			}
		}
		send(sock, "*", 1, 0);
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
			if (clntSocks.find(temp)->second.state != NONE) {
				send(sock, "2", 1, 0);
				send(sock, "T", 1, 0);
				return;
			}
			buf[0] = 2;
			send(temp, buf, 1, 0);
			send(temp, clntSocks.find(sock)->second.name, strlen(name), 0);
			clntSocks.find(temp)->second.state = WaitingAnswer;
			clntSocks.find(sock)->second.state = WaitingRequest;
			requests.push_back(make_pair(temp, sock));
		}
		else {
			send(sock, "2", 1, 0);
			send(sock, "I", 1, 0);			//Invalid
		}
	}

	else if (strncmp(msg, "makegroup", 9) == 0) {
		vector<SOCKET> v_temp;
		vector<char*> v_temp2;
		num = rand();
		while (group_chats.find(num) != group_chats.end()) num = rand();
		v_temp.push_back(sock);
		group_chats.insert(make_pair(num, v_temp));
		group_announce.insert(make_pair(num, v_temp2));
		clntSocks.find(sock)->second.state = GroupConnected;
		clntSocks.find(sock)->second.connectwith = num;
		buf[0] = (char)3;
		send(sock, buf, 1, 0);
		sprintf_s(buf, "%d", num);
		send(sock, buf, strlen(buf) + 1, 0);
	}

	else if (strncmp(msg, "joingroup", 9) == 0) {
		auto room = group_chats.find(atoi(msg + 9));
		if (room != group_chats.end()) {
			room->second.push_back(sock);
			clntSocks.find(sock)->second.state = GroupConnected;
			clntSocks.find(sock)->second.connectwith = room->first;
			buf[0] = (char)4;
			send(sock, buf, 1, 0);
			send(sock, "Y", 1, 0);
			
			sprintf_s(buf, "%s 님이 입장하였습니다.\n", clntSocks.find(sock)->second.name);
			SendGroupMsg(sock, buf, strlen(buf));
		}
		else {
			buf[0] = (char)4;
			send(sock, buf, 1, 0);
			send(sock, "N", 1, 0);
		}
	}
}

void breakroom(SOCKET sock) {
	char msg[] = "q";
	WaitForSingleObject(hMutex, INFINITE);
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
	ReleaseMutex(hMutex);
}

int group_exit(SOCKET sock) {
	auto room = group_chats.find(clntSocks.find(sock)->second.connectwith);
	for (auto it = room->second.begin(); it != room->second.end(); it++) {
		if (*it == sock) {
			room->second.erase(it);
			break;
		}
	}
	setSocketState(sock, NONE);
	if (room->second.size() == 0) {
		auto& announce = group_announce.find(clntSocks.find(sock)->second.connectwith)->second;
		for (auto it = announce.begin(); it != announce.end(); it++) {
			free(*it);
		}
		group_announce.erase(clntSocks.find(sock)->second.connectwith);
		group_chats.erase(room);
		return 1;
	}
	return 0;;
}

int setSocketState(SOCKET s, enum States state) {
	WaitForSingleObject(hMutex, INFINITE);
	auto it = clntSocks.find(s);
	int ret = 1;
	if (it != clntSocks.end()) {
		it->second.state = state;
		ret = 0;
	}
	ReleaseMutex(hMutex);
	return ret;
}

void SendMsg(SOCKET sock, char* msg, int len)
{
	WaitForSingleObject(hMutex, INFINITE);
	for (auto it = chatrooms.begin(); it != chatrooms.end(); it++) {
		if (it->first == sock) {
			sendMsgWithProtocol(it->second, msg);
			break;
		}
		else if (it->second == sock) {
			sendMsgWithProtocol(it->first, msg);
			break;
		}
	}

	ReleaseMutex(hMutex);
}

int isGroupCommand(SOCKET sock, char* msg) {
	if (strncmp(msg, "!h", 2) == 0 || strncmp(msg, "!H", 2) == 0) {
		strcpy_s(msg, BUF_SIZE, "!l or !L : 채팅방 멤버 리스트\n");
		if (sock == group_chats.find(clntSocks.find(sock)->second.connectwith)->second[0]) {
			strcat_s(msg, BUF_SIZE, "!o or !O : 멤버 강제 퇴장\n!a or !A : 채팅방 공지 등록\n");
		}
		strcat_s(msg, BUF_SIZE, "!c or !C : 채팅방 공지 확인\n!e or !E : 대화방 나가기\n");
		sendMsgWithProtocol(sock, msg);
		return 1;
	}
	else if (strncmp(msg, "!l", 2) == 0 || strncmp(msg, "!L", 2) == 0) {
		auto room = group_chats.find(clntSocks.find(sock)->second.connectwith);
		msg[0] = '\0';
		for (auto it = room->second.begin(); it != room->second.end(); it++) {
			strcat_s(msg, BUF_SIZE, clntSocks.find(*it)->second.name);
			strcat_s(msg, BUF_SIZE, "\n");
		}
		sendMsgWithProtocol(sock, msg);
		return 1;
	}
	else if (sock == group_chats.find(clntSocks.find(sock)->second.connectwith)->second[0] && (strncmp(msg, "!a", 2) == 0 || strncmp(msg, "!A", 2) == 0)) {
		strcpy_s(msg, BUF_SIZE, "공지를 등록해주세요.\n");
		sendMsgWithProtocol(sock, msg);
		setSocketState(sock, GroupAnnounce);
		return 1;
	}
	else if (strncmp(msg, "!c", 2) == 0 || strncmp(msg, "!C", 2) == 0) {
		send(sock, "\n", 1, 0);
		auto announce_v = group_announce.find(clntSocks.find(sock)->second.connectwith)->second;
		msg[0] = (char)announce_v.size();
		send(sock, msg, 1, 0);
		
		for (auto it = announce_v.begin(); it != announce_v.end(); it++) {
			send(sock, *it, strlen(*it), 0);
			send(sock, "\n", 1, 0);
		}
		send(sock, "\b", 1, 0);
		return 1;
	}
	return 0;
}

void sendMsgWithProtocol(SOCKET sock, char* msg) {
	char num[1];
	int n = strlen(msg) + 1;
	num[0] = (char)(n / 100);;
	send(sock, num, 1, 0);
	num[0] = (char)(n % 100);
	send(sock, num, 1, 0);
	send(sock, msg, strlen(msg) + 1, 0);
}

void SendGroupMsg(SOCKET sock, char* msg, int len)
{
	WaitForSingleObject(hMutex, INFINITE);
	int roomnum = clntSocks.find(sock)->second.connectwith;
	auto room = group_chats.find(roomnum);
	for (auto it = room->second.begin(); it != room->second.end(); it++) {
		if (*it == sock) continue;
		sendMsgWithProtocol(*it, msg);
	}
	ReleaseMutex(hMutex);
}

void ErrorHandling(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}