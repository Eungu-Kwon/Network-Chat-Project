#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1000
#define NAME_SIZE 100

void setName(SOCKET serv);
enum Menu getCommand();
void printCommands();
void printList(SOCKET hSock);
void makeGroup(SOCKET hSock);
void joinGroup(SOCKET hSock);
void chatRequest(SOCKET hSock);
int runCommand(SOCKET hSock);

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);
void ErrorHandling(const char* msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

HANDLE hEvent, hEventForList;

enum Menu{
	Menu_ShowCommands = 1,
	Menu_CheckList,
	Menu_ChatRequest,
	Menu_MakeGroup,
	Menu_JoinGroup,
	Menu_Exit,
	REQUEST_YES,
	REQUEST_NO,
	Menu_Bad
};

enum State {
	NONE,
	WaitingRequest,
	WaitingAnswer,
	WaitingServer,
	Connected,
	GroupConnected
};

enum State state = NONE;

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET hSock;
	SOCKADDR_IN servAdr;
	HANDLE hSndThread, hRcvThread;
	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	hSock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr(argv[1]);
	servAdr.sin_port = htons(atoi(argv[2]));

	if (connect(hSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	setName(hSock);

	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	hEventForList = CreateEvent(NULL, TRUE, FALSE, NULL);

	printf("ä�� ������ ���ӵǾ����ϴ�.\n");

	hRcvThread =
		(HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSock, 0, NULL);

	while (1) {
		if (state == NONE || state == WaitingAnswer) {
			if (runCommand(hSock)) break;
		}

		else if (state == WaitingRequest) WaitForSingleObject(hEvent, INFINITE);

		else if (state == Connected || state == GroupConnected) {
			if (state == Connected) printf("����Ǿ����ϴ�.\n");
			hSndThread =
				(HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSock, 0, NULL);
			WaitForSingleObject(hSndThread, INFINITE);
		}
	}

	closesocket(hSock);
	WSACleanup();
	return 0;
}

void setName(SOCKET serv) {
	char buf[10];
	fputs("ä�� ������ �����ϱ� ���� �г����� �Է����ּ���.\n", stdout);
	while (1) {
		cin >> msg;
		cin.ignore(1);

		sprintf(name, "[%s]", msg);

		send(serv, name, strlen(name), 0);
		recv(serv, buf, 1, 0);
		if (buf[0] == 'Y') break;
		else {
			fputs("������ �̸��� �����մϴ�. �ٸ��̸����� �������ּ���.\n", stdout);
		}
	}
}

enum Menu getCommand() {
	if (state == NONE) fputs("�޴��� �������ּ���. (!h or !H : ��ɾ� Ȯ��)\n", stdout);
	else if (state == WaitingAnswer) fputs("��ɾ �߸��Ǿ����ϴ�. �ٽ� �Է����ּ��� (y/n)\n", stdout);

	cin >> msg;
	cin.ignore(1);

	if (state == NONE) {
		if (strcmp(msg, "!h") == 0 || strcmp(msg, "!H") == 0) return Menu_ShowCommands;
		else if (strcmp(msg, "!l") == 0 || strcmp(msg, "!L") == 0) return Menu_CheckList;
		else if (strcmp(msg, "!r") == 0 || strcmp(msg, "!R") == 0) return Menu_ChatRequest;
		else if (strcmp(msg, "!g") == 0 || strcmp(msg, "!G") == 0) return Menu_MakeGroup;
		else if (strcmp(msg, "!j") == 0 || strcmp(msg, "!J") == 0) return Menu_JoinGroup;
		else if (strcmp(msg, "!q") == 0 || strcmp(msg, "!Q") == 0) return Menu_Exit;
		else return Menu_Bad;
	}
	else if (state == WaitingAnswer) {
		if ((strcmp(msg, "y") == 0 || strcmp(msg, "Y") == 0)) return REQUEST_YES;
		else if ((strcmp(msg, "n") == 0 || strcmp(msg, "N") == 0)) return REQUEST_NO;
		else return Menu_Bad;
	}

	else return Menu_Bad;
}

void printCommands() {
	printf("!h or !H : ��ɾ� Ȯ��\n");
	printf("!l or !L : ����� ����Ʈ Ȯ��\n");
	printf("!r or !R : 1 �� 1 ä�� ��û\n");
	printf("!g or !G : ��ü ä�ù� �����\n");
	printf("!j or !J : ��ü ä�ù� ����\n");
	printf("!e or !E : ��ȭ�� ä�� ����\n");
	printf("!q or !Q : ���α׷� ����\n");
}

void printList(SOCKET hSock) {
	send(hSock, "showlist", 9, 0);
	WaitForSingleObject(hEventForList, INFINITE);
	ResetEvent(hEventForList);
}

void chatRequest(SOCKET hSock) {
	char nameBuf[100];
	char msgToSend[111];
	printf("����� �Է��ϼ���. >> ");
	cin >> nameBuf;
	cin.ignore(1);
	if (strncmp(nameBuf, name + 1, strlen(name) - 2) == 0) {
		printf("�ڱ��ڽſ��� ��û ���� �� �����ϴ�!\n\n");
		return;
	}
	printf("��û��...\n");
	sprintf_s(msgToSend, "requestchat%s", nameBuf);
	send(hSock, msgToSend, strlen(msgToSend), 0);
	ResetEvent(hEvent);
	state = WaitingRequest;
}

void makeGroup(SOCKET hSock) {
	send(hSock, "makegroup", 10, 0);
	WaitForSingleObject(hEventForList, INFINITE);
	ResetEvent(hEventForList);
}

void joinGroup(SOCKET hSock) {
	char temp[BUF_SIZE];
	printf("��ü ä�ù濡 �����ϱ� ���� �ڵ带 �Է����ּ���.\n");
	cin >> msg;
	cin.ignore(1);
	sprintf(temp, "joingroup%s", msg);
	send(hSock, temp, strlen(temp) + 1, 0);
	WaitForSingleObject(hEventForList, INFINITE);
	ResetEvent(hEventForList);
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
		chatRequest(hSock);
		break;
	case Menu_MakeGroup:
		makeGroup(hSock);
		break;
	case Menu_JoinGroup:
		joinGroup(hSock);
		break;
	case Menu_Exit:
		printf("���α׷��� �����մϴ�.\n");
		return 1;
	case REQUEST_YES:
		send(hSock, "Y", 1, 0);
		state = WaitingServer;
		break;
	case REQUEST_NO:
		send(hSock, "N", 1, 0);
		state = NONE;
		break;
	default:
		break;
	}
	return 0;
}

unsigned WINAPI SendMsg(void* arg) 
{
	SOCKET hSock = *((SOCKET*)arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];
	while (1)
	{
		fgets(msg, BUF_SIZE, stdin);
		if (state != Connected && state != GroupConnected) break;
		if (!strcmp(msg, "!e\n") || !strcmp(msg, "!E\n"))
		{
			strcpy(msg, "!!\n//quit//!!");
			send(hSock, msg, 13, 0);
			state = NONE;
			break;
		}
		send(hSock, msg, strlen(msg) - 1, 0);
	}
	
	return 0;
}

unsigned WINAPI RecvMsg(void* arg) 
{
	int hSock = *((SOCKET*)arg);
	char msg[NAME_SIZE + BUF_SIZE];
	int strLen;
	int bufInt;
	int n, m;
	while (1)
	{
		strLen = recv(hSock, msg, 1, 0);
		if (strLen == -1)
			return -1;
		bufInt = (int)msg[0];

		if (state == NONE) {
			switch (bufInt)
			{
			case 1:											// Get Name List
				while (recv(hSock, msg, 1, 0) > 0) {
					if (msg[0] == '\b') break;
					printf("%c", msg[0]);
				}
				printf("\n");
				SetEvent(hEventForList);
				break;
			case 2:
				strLen = recv(hSock, msg, BUF_SIZE + NAME_SIZE, 0);
				msg[strLen] = '\0';
				printf("%s ���� ��û�Ͽ����ϴ�. �����ðڽ��ϱ�? (y : ����, n : ����)\n", msg);
				state = WaitingAnswer;
				break;
			case 3:
				strLen = recv(hSock, msg, sizeof(msg), 0);
				printf("��ü ä�ù� �Է��ڵ�� %s �Դϴ�. (!h or !H : ��ü ä�ù� ��ɾ� Ȯ��)\n\n", msg);
				state = GroupConnected;
				SetEvent(hEventForList);
				break;
			case 4:
				strLen = recv(hSock, msg, 1, 0);
				if (msg[0] == 'Y') {
					printf("��ü ä�ù濡 �����Ͽ����ϴ�. (!h or !H : ��ü ä�ù� ��ɾ� Ȯ��)\n\n");
					state = GroupConnected;
				}
				else {
					printf("�������� �ʴ� �ڵ��Դϴ�. �ٽ� Ȯ�����ּ���.\n\n");
				}
				SetEvent(hEventForList);
				break;
			default:
				break;
			}
		}
		
		else if (state == WaitingRequest) {
			if(msg[0] == 'Y') state = Connected;
			else if (msg[0] == 'N') {
				printf("���κ��� �������߽��ϴ�...\n\n");
				state = NONE;
			}
			else if (msg[0] == 'I') {
				printf("�������� �ʴ� ������Դϴ�. �ٽ� Ȯ�����ּ���.\n\n");
				state = NONE;
			}
			else if (msg[0] == 'T') {
				printf("�ش� ����ڴ� ���� ��û�� �� �����ϴ�.\n\n");
				state = NONE;
			}
			SetEvent(hEvent);
		}

		else if (state == GroupConnected && msg[0] == '\n') {
			recv(hSock, msg, 1, 0);
			int a_count = (int)msg[0];
			if (a_count == 0) printf("���� ���������� �����ϴ�!\n");

			while (recv(hSock, msg, 1, 0) > 0) {
				if (msg[0] == '\b') break;
				printf("%c", msg[0]);
			}
		}

		else if (state == Connected || state == GroupConnected) {
			if (msg[0] == 'q') {
				state = NONE;
				printf("������ ���������ϴ�. ����Ϸ��� ���͸� ��������.");
				continue;
			}
			if (msg[0] == 'o') {
				state = NONE;
				printf("���忡 ���� ������߽��ϴ�. ����Ϸ��� ���͸� ��������.");
				continue;
			}
			if (msg[0] == 'Y') {
				printf("������׽��ϴ�.\n\n");
				continue;
			}
			else if (msg[0] == 'N') {
				printf("�ش� ����ڴ� �������� �ʽ��ϴ�. �ٽ� Ȯ�����ּ���.\n\n");
				continue;
			}
			else if (msg[0] == 'M') {
				printf("�ڱ� �ڽ��� ������ �� �����ϴ�!\n\n");
				continue;
			}
			n = bufInt;
			recv(hSock, msg, 1, 0);
			m = msg[0];
			strLen = recv(hSock, msg, (n * 100) + m, 0);
			msg[strLen] = '\0';
			printf("%s", msg);
		}

		else if (state == WaitingServer) {
			if (msg[0] == 'O') {
				state = Connected;
			}
			else {
				printf("���ῡ ������ ������ϴ�. �ٽ� �õ����ּ���.\n\n");
				state = NONE;
			}
		}
	}
	return 0;
}

void ErrorHandling(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
