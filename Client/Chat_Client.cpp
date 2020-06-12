#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 100
#define NAME_SIZE 20

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);
void ErrorHandling(const char* msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

enum Menu{
	Menu_ShowCommands = 1,
	Menu_CheckList,
	Menu_ChatRequest,
	Menu_MakeGroup,
	Menu_JoinGroup,
	Menu_Exit,
	Menu_Bad
};

void setName() {
	int name_size;
	fputs("채팅 서버에 접속하기 위해 닉네임을 입력해주세요.\n", stdout);
	cin >> msg;
	cin.ignore(1);
	sprintf(name, "[%s]", msg);
}

enum Menu getCommand() {
	fputs("메뉴를 선택해주세요. (!h or !H : 명령어 확인)\n", stdout);
	cin >> msg;
	cin.ignore(1);
	if (strcmp(msg, "!h") == 0 || strcmp(msg, "!H") == 0) return Menu_ShowCommands;
	else if (strcmp(msg, "!l") == 0 || strcmp(msg, "!L") == 0) return Menu_CheckList;
	else if (strcmp(msg, "!r") == 0 || strcmp(msg, "!R") == 0) return Menu_ChatRequest;
	else if (strcmp(msg, "!g") == 0 || strcmp(msg, "!G") == 0) return Menu_MakeGroup;
	else if (strcmp(msg, "!j") == 0 || strcmp(msg, "!J") == 0) return Menu_JoinGroup;
	else if (strcmp(msg, "!q") == 0 || strcmp(msg, "!Q") == 0) return Menu_Exit;
	else return Menu_Bad;
}

void printCommands() {
	cout << "!h or !H : 명령어 확인" << '\n';
	cout << "!l or !L : 사용자 리스트 확인" << '\n';
	cout << "!r or !R : 1 대 1 채팅 요청" << '\n';
	cout << "!g or !G : 단체 채팅방 만들기" << '\n';
	cout << "!j or !J : 단체 채팅방 입장" << '\n';
	cout << "!q or !Q : 프로그램 종료" << '\n';
}

void printList(SOCKET hSock) {
	char names[100];
	int nameSize;
	send(hSock, "showlist", 9, 0);
	while (1) {
		nameSize = recv(hSock, names, sizeof(names), 0);
		names[nameSize] = '\0';
		if (strcmp(names, "/end") == 0) {
			cout << '\n';
			break;
		}
		cout << names << '\n';
	}
}

int runCommand(SOCKET hSock) {
	switch (getCommand())
	{
	case Menu_ShowCommands:
		printCommands();
		break;
	case Menu_CheckList:
		printList(hSock);
		break;
	case Menu_ChatRequest:
		break;
	case Menu_MakeGroup:
		break;
	case Menu_JoinGroup:
		break;
	case Menu_Exit:
		cout << "프로그램을 종료합니다." << '\n';
		return 1;
	default:
		break;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET hSock;
	SOCKADDR_IN servAdr;
	HANDLE hSndThread, hRcvThread;
	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		//exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	setName();
	hSock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servAdr.sin_port = htons(atoi("5555"));

	if (connect(hSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	send(hSock, name, strlen(name), 0);
	cout << "채팅 서버에 접속되었습니다." << endl;

	while (1) {
		if (runCommand(hSock)) break;
	}

	hSndThread =
		(HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSock, 0, NULL);
	hRcvThread =
		(HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSock, 0, NULL);

	WaitForSingleObject(hSndThread, INFINITE);
	WaitForSingleObject(hRcvThread, INFINITE);
	closesocket(hSock);
	WSACleanup();
	return 0;
}

unsigned WINAPI SendMsg(void* arg)   // send thread main
{
	SOCKET hSock = *((SOCKET*)arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];
	while (1)
	{
		fgets(msg, BUF_SIZE, stdin);
		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))
		{
			closesocket(hSock);
			exit(0);
		}
		//sprintf(nameMsg, "%s %s", name, msg);
		send(hSock, msg, strlen(msg), 0);
	}
	return 0;
}

unsigned WINAPI RecvMsg(void* arg)   // read thread main
{
	int hSock = *((SOCKET*)arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];
	int strLen;
	while (1)
	{
		strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);
		if (strLen == -1)
			return -1;
		nameMsg[strLen] = 0;
		fputs(nameMsg, stdout);
	}
	return 0;
}

void ErrorHandling(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
