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

int state = 0;

HANDLE hMutex, hMutex1;

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

void setName() {
	int name_size;
	fputs("ä�� ������ �����ϱ� ���� �г����� �Է����ּ���.\n", stdout);
	cin >> msg;
	cin.ignore(1);
	sprintf(name, "[%s]", msg);
}

enum Menu getCommand() {
	fputs("�޴��� �������ּ���. (!h or !H : ��ɾ� Ȯ��)\n", stdout);
	
	cin >> msg;
	cin.ignore(1);
	if (strcmp(msg, "!h") == 0 || strcmp(msg, "!H") == 0) return Menu_ShowCommands;
	else if (strcmp(msg, "!l") == 0 || strcmp(msg, "!L") == 0) return Menu_CheckList;
	else if (strcmp(msg, "!r") == 0 || strcmp(msg, "!R") == 0) return Menu_ChatRequest;
	else if (strcmp(msg, "!g") == 0 || strcmp(msg, "!G") == 0) return Menu_MakeGroup;
	else if (strcmp(msg, "!j") == 0 || strcmp(msg, "!J") == 0) return Menu_JoinGroup;
	else if (strcmp(msg, "!q") == 0 || strcmp(msg, "!Q") == 0) return Menu_Exit;
	else if (state == 1 && (strcmp(msg, "!y") == 0 || strcmp(msg, "!Y") == 0)) return REQUEST_YES;
	else if (state == 1 && (strcmp(msg, "!n") == 0 || strcmp(msg, "!N") == 0)) return REQUEST_NO;
	else return Menu_Bad;
}

void printCommands() {
	cout << "!h or !H : ��ɾ� Ȯ��" << '\n';
	cout << "!l or !L : ����� ����Ʈ Ȯ��" << '\n';
	cout << "!r or !R : 1 �� 1 ä�� ��û" << '\n';
	cout << "!g or !G : ��ü ä�ù� �����" << '\n';
	cout << "!j or !J : ��ü ä�ù� ����" << '\n';
	cout << "!q or !Q : ���α׷� ����" << '\n';
}

void printList(SOCKET hSock) {
	char buf[1];
	int nameSize, nameCount;
	
	send(hSock, "showlist", 9, 0);
	recv(hSock, buf, 1, 0);
	
}

void chatRequest(SOCKET hSock) {
	char name[100];
	cout << "����� �Է��ϼ���. >> ";
	cin >> name;
	cin.ignore(1);
	cout << "��û��..." << '\n';
	send(hSock, "requestchat", 12, 0);
	send(hSock, name, strlen(name), 0);
	state = 2;
}

int runCommand(SOCKET hSock) {
	WaitForSingleObject(hMutex, INFINITE);
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
		break;
	case Menu_JoinGroup:
		break;
	case Menu_Exit:
		cout << "���α׷��� �����մϴ�." << '\n';
		return 1;
	case REQUEST_YES:
		send(hSock, "Y", 1, 0);
		state = 3;
		break;
	case REQUEST_NO:
		send(hSock, "N", 1, 0);
		state = 0;
		break;
	default:
		return 0;
	}
	ReleaseMutex(hMutex);
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

	hMutex = CreateMutex(NULL, FALSE, NULL);
	hMutex1 = CreateMutex(NULL, FALSE, NULL);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servAdr.sin_port = htons(atoi("5555"));

	if (connect(hSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	send(hSock, name, strlen(name), 0);
	cout << "ä�� ������ ���ӵǾ����ϴ�." << endl;

	hRcvThread =
		(HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSock, 0, NULL);

	while (1) {
		if (state == 0) {
			if (runCommand(hSock)) break;
		}
		
		if (state == 3) {
			cout << "����Ǿ����ϴ�.\n";
			hSndThread =
				(HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSock, 0, NULL);
			WaitForSingleObject(hSndThread, INFINITE);
		}
	}

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
		if (state == 0) break;
		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))
		{
			send(hSock, msg, 1, 0);
			break;
		}
		send(hSock, msg, strlen(msg) - 1, 0);
	}
	state = 0;
	return 0;
}

unsigned WINAPI RecvMsg(void* arg)   // read thread main
{
	int hSock = *((SOCKET*)arg);
	char msg[NAME_SIZE + BUF_SIZE];
	int strLen;
	int bufInt;
	while (1)
	{
		strLen = recv(hSock, msg, 1, 0);

		//printf("you got message\n");
		
		if (strLen == -1)
			return -1;
		
		bufInt = (int)msg[0];

		if (state == 0) {
			switch (bufInt)
			{
			case 1:
				WaitForSingleObject(hMutex, INFINITE);
				strLen = recv(hSock, msg, 1, 0);
				if (strLen == -1)
					return -1;
				bufInt = (int)msg[0];
				while (bufInt != 0) {
					if (recv(hSock, msg, 1, 0) < 0) break;
					if (msg[0] == '\n') bufInt--;
					printf("%c", msg[0]);
				}
				cout << endl;
				ReleaseMutex(hMutex);
				break;
			case 2:
				strLen = recv(hSock, msg, BUF_SIZE + NAME_SIZE, 0);
				msg[strLen] = '\0';
				cout << msg << " ���� ��û�Ͽ����ϴ�.\n";
				state = 1;
			default:
				break;
			}
		}
		
		else if (state == 2) {
			if(msg[0] == 'Y') state = 3;
			else if(msg[0] == 'N') state = 0;
		}

		else if (state == 3) {
			if (msg[0] == 'q') {
				state = 0;
				cout << "������ ���������ϴ�. ����Ϸ��� ���͸� ��������.";
				continue;
			}
			strLen = recv(hSock, msg, BUF_SIZE, 0);
			msg[strLen] = '\0';
			cout << msg;
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
