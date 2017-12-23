// Proxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Proxy.h"
#include "afxsock.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HTTP "http://"
#define PORT 8888
#define HTTPPORT 80
#define BSIZE 10000


// The one and only application object

CWinApp theApp;

using namespace std;
int global = 0;
struct SocketPair {
	SOCKET Server;
	SOCKET Client;
	bool IsServerClosed;
	bool IsClientClosed;
};

struct Param {
	string address;
	HANDLE handle;
	SocketPair *pair;
	int port;
};

void StartServer();
UINT ClientToProxy(void *lParam);
UINT ProxyToServer(void *lParam);
void CloseServer();
UINT GetKeyDown(void *lParam);
void GetAddrNPort(string &buf, string &address, int &port);
void Split(string str, vector<string> &cont, char delim = ' ');
//Ref: http://stackoverflow.com/questions/19715144/how-to-convert-char-to-lpcwstr
wchar_t *convertCharArrayToLPCWSTR(const char* charArray);
void LoadBlackList(vector<string> &arr);
bool CheckServerName(string server_name);
sockaddr_in* GetServer(string server_name,char*hostname);

SOCKET Listen;
typedef SOCKET Socket;
vector<string> black_list;
string ResForbidden = "HTTP/1.0 403 Forbidden\r\n\Cache-Control: no-cache\r\n\Connection: close\r\n";
Socket gListen;
bool run = 1;

int main()
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			wprintf(L"Fatal Error: MFC initialization failed\n");
			nRetCode = 1;
		}
		else
		{

			StartServer();
			while (run);
			CloseServer();
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		wprintf(L"Fatal Error: GetModuleHandle failed\n");
		nRetCode = 1;
	}

	return nRetCode;
}

void StartServer()
{
	sockaddr_in local;
	Socket listen_socket;
	WSADATA wsaData;
	if (WSAStartup(0x202, &wsaData) != 0)
	{
		cout << "\nLoi khoi tao socket\n";
		WSACleanup(); 
		return;
	}
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(PORT);
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "\nSocket khoi tao khong hop le.";
		WSACleanup();
		return;
	}
	if (bind(listen_socket, (sockaddr *)&local, sizeof(local)) != 0)
	{
		cout << "\n Loi khi bind socket.";
		WSACleanup();
		return;
	};

	if (listen(listen_socket, 5) != 0)
	{
		cout << "\n Loi khi nghe."; 
		WSACleanup(); 
		return;
	}
	LoadBlackList(black_list);
	if (black_list.size() == 0) {
		cout << "Khong the load duoc blacklist" << endl;
	}
	gListen = listen_socket;
	AfxBeginThread(ClientToProxy, (LPVOID)listen_socket);
	CWinThread* p = AfxBeginThread(GetKeyDown, &run);
	WaitForSingleObject(p->m_hThread, 100);
}

UINT ClientToProxy(void * lParam)
{
	Socket socket = (Socket)lParam;
	SocketPair Pair;
	SOCKET SClient;
	sockaddr_in addr;
	int addrLen = sizeof(addr);
	SClient = accept(socket, (sockaddr*)&addr, &addrLen);
	AfxBeginThread(ClientToProxy, lParam);
	char Buffer[BSIZE];
	int Len;
	Pair.IsServerClosed = FALSE;
	Pair.IsClientClosed = FALSE;
	Pair.Client = SClient;
	int retval = recv(Pair.Client, Buffer, BSIZE, 0);
	if (retval == SOCKET_ERROR) {
		cout << "\nloi khi nhan yeu cau" << endl;
		if (!Pair.IsClientClosed) {
			closesocket(Pair.Client);
			Pair.IsClientClosed = TRUE;
		}
	}
	if (retval == 0) {
		cout << "\nclient ngat ket noi" << endl;
		if (!Pair.IsClientClosed) {
			closesocket(Pair.Client);
			Pair.IsClientClosed = TRUE;
		}
	}
	retval >= BSIZE ? Buffer[retval - 1] = 0 : (retval > 0 ? Buffer[retval] = 0 : Buffer[0] = 0);
	cout << "\n Client received " << retval << "data :\n[" << Buffer << "]";
	string buf(Buffer), address;
	int port;
	GetAddrNPort(buf, address, port);
	bool check = FALSE;
	if (!CheckServerName(address)) {
		retval = send(Pair.Client, ResForbidden.c_str(), ResForbidden.size(), 0);
		check = TRUE;
		Sleep(2000);
	}
	Param P;
	P.handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	P.address = address;
	P.port = port;
	P.pair = &Pair;
	if (check == FALSE) {
		CWinThread* pThread = AfxBeginThread(ProxyToServer, (LPVOID)&P);
		WaitForSingleObject(P.handle, 6000);
		CloseHandle(P.handle);
		while (Pair.IsClientClosed == FALSE
			&& Pair.IsServerClosed == FALSE) {
			retval = send(Pair.Server, buf.c_str(), buf.size(), 0);
			if (retval == SOCKET_ERROR) {
				cout << "Send Failed, Error: " << GetLastError();
				if (Pair.IsServerClosed == FALSE) {
					closesocket(Pair.Server);
					Pair.IsServerClosed = TRUE;
				}
				continue;
			}
			retval = recv(Pair.Client, Buffer, BSIZE, 0);
			if (retval == SOCKET_ERROR) {
				cout << "C Receive Failed, Error: " << GetLastError();
				if (Pair.IsClientClosed == FALSE) {
					closesocket(Pair.Client);
					Pair.IsClientClosed = TRUE;
				}
				continue;
			}
			if (retval == 0) {
				cout << "Client closed " << endl;
				if (Pair.IsClientClosed == FALSE) {
					closesocket(Pair.Server);
					Pair.IsClientClosed = TRUE;
				}
				break;
			}
			retval >= BSIZE ? Buffer[retval - 1] = 0 : (retval > 0 ? Buffer[retval] = 0 : Buffer[0] = 0);
			cout << "\n Client received " << retval << "data :\n[" << Buffer << "]";
		}
		if (Pair.IsServerClosed == FALSE) {
			closesocket(Pair.Server);
			Pair.IsServerClosed = TRUE;
		}
		if (Pair.IsClientClosed == FALSE) {
			closesocket(Pair.Client);
			Pair.IsClientClosed = TRUE;
		}
		WaitForSingleObject(pThread->m_hThread, 20000);
	}
	else {
		if (Pair.IsClientClosed == FALSE) {
			closesocket(Pair.Client);
			Pair.IsClientClosed = TRUE;
		}
	}
	return 0;
}

UINT ProxyToServer(void * lParam)
{
	int count = 0;
	Param *P = (Param*)lParam;
	string server_name = P->address;
	int port = P->port;
	int status;
	int addr;
	char hostname[32] = "";
	sockaddr_in *server = NULL;
	cout << "Server Name: " << server_name << endl;
	server = GetServer(server_name, hostname);
	if (server == NULL) {
		cout << "\n Khong the lay dia chi IP" << endl;
		send(P->pair->Client, ResForbidden.c_str(), ResForbidden.size(), 0);
		return -1;
	}
	if (strlen(hostname) > 0) {
		cout << "connecting to:" << hostname << endl;

		int retval;
		char Buffer[BSIZE];
		Socket Server;
		Server = socket(AF_INET, SOCK_STREAM, 0);
		if (!(connect(Server, (sockaddr*)server, sizeof(sockaddr)) == 0)) {
			cout << "Khong the ket noi";
			send(P->pair->Client, ResForbidden.c_str(), ResForbidden.size(), 0);

			return -1;
		}
		else {
			cout << "Ket noi thanh cong \n";
			P->pair->Server = Server;
			P->pair->IsServerClosed == FALSE;
			SetEvent(P->handle);
			int c = 0;
			while (P->pair->IsClientClosed == FALSE &&
				P->pair->IsServerClosed == FALSE) {
				retval = recv(P->pair->Server, Buffer, BSIZE, 0);
				if (retval == SOCKET_ERROR) {
					closesocket(P->pair->Server);
					P->pair->IsServerClosed = TRUE;
					break;
				}
				if (retval == 0) {
					cout << "\nServer Closed" << endl;
					closesocket(P->pair->Server);
					P->pair->IsServerClosed = TRUE;
				}
				retval = send(P->pair->Client, Buffer, retval, 0);
				if (retval == SOCKET_ERROR) {
					cout << "\nSend Failed, Error: " << GetLastError();
					closesocket(P->pair->Client);
					P->pair->IsClientClosed = TRUE;
					break;
				}
				retval >= BSIZE ? Buffer[retval - 1] = 0 : Buffer[retval] = 0;
				cout << "\n Server received " << retval << "data :\n[" << Buffer << "]";
				ZeroMemory(Buffer, BSIZE);
			}

			if (P->pair->IsClientClosed == FALSE) {
				closesocket(P->pair->Client);
				P->pair->IsClientClosed = TRUE;
			}
			if (P->pair->IsServerClosed == FALSE) {
				closesocket(P->pair->Server);
				P->pair->IsServerClosed = TRUE;
			}
		}
	}
	return 0;
}

void CloseServer()
{
	closesocket(gListen);
	WSACleanup();
}

UINT GetKeyDown(void * lParam)
{
	bool * run = (bool*)lParam;
	while (*run) {
		char c;
		c = getchar();
		if (c == 'q') {
			*run = 0;
		}
	}
	return 0;
}

void GetAddrNPort(string &buf, string &address, int &port)
{
	vector<string> cont;
	//cont 0: command, 1: link, 2: proto
	Split(buf, cont);
	if (cont.size() > 0) {
		int pos = cont[1].find(HTTP);
		if (pos != -1) {
			string add = cont[1].substr(pos + strlen(HTTP));
			address = add.substr(0, add.find('/'));
			port = 80;
			string temp;
			int len = strlen(HTTP) + address.length();
			while (len > 0) {
				temp.push_back(' ');
				len--;
			}
			buf = buf.replace(buf.find(HTTP + address), strlen(HTTP) + address.length(), temp);
		}
	}
}

void Split(string str, vector<string> &cont, char delim)
{
	istringstream ss(str);
	string token;
	while (getline(ss, token, delim)) {
		cont.push_back(token);
	}
}

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

void LoadBlackList(vector<string>& arr)
{
	fstream f;
	f.open("blacklist.conf", ios::in | ios::out);
	if (f.is_open()) {
		while (!f.eof()) {
			string temp;
			getline(f, temp);
			if (temp.back() == '\n') {
				temp.pop_back();
			}
			arr.push_back(temp);
		}
	}
}

bool CheckServerName(string server_name) {
	if (black_list.size() > 0) {
		for (auto i : black_list)
		{
			if (i.find(server_name) != string::npos) {
				cout << i.find(server_name);
				return 0;
			}
		}
	}
	return 1;
}

sockaddr_in* GetServer(string server_name,char * hostname)
{
	int status;
	sockaddr_in *server = NULL;
	if (server_name.size() > 0) {
		if (isalpha(server_name.at(0))) {
			addrinfo hints, *res = NULL;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			if ((status = getaddrinfo(server_name.c_str(), "80", &hints, &res)) != 0) {
				printf("getaddrinfo failed: %s", gai_strerror(status));
				return NULL;
			}
			while (res->ai_next != NULL) {
				res = res->ai_next;
			}
			sockaddr_in * temp = (sockaddr_in*)res->ai_addr;
			inet_ntop(res->ai_family, &temp->sin_addr, hostname, 32);
			server = (sockaddr_in*)res->ai_addr;
			unsigned long addr;
			inet_pton(AF_INET, hostname, &addr);
			server->sin_addr.s_addr = addr;
			server->sin_port = htons(80);
			server->sin_family = AF_INET;
		}
		else {
			unsigned long addr;
			inet_pton(AF_INET, server_name.c_str(), &addr);
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = addr;
			if ((status = getnameinfo((sockaddr*)&sa,
				sizeof(sockaddr), hostname, NI_MAXHOST, NULL, NI_MAXSERV, NI_NUMERICSERV)) != 0) {
				cout << "Error";
				return NULL;
			}
			server->sin_addr.s_addr = addr;
			server->sin_family = AF_INET;
			server->sin_port = htons(80);
		}
	}

	return server;
}
