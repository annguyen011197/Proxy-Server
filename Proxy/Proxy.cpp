// Proxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Proxy.h"
#include "afxsock.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HTTP "http://"
#define PORT 8081
#define HTTPPORT 80


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
void GetAddrNPort(string &buf, string &address, int &port);
void Split(string str, vector<string> &cont, char delim = ' ');
//Ref: http://stackoverflow.com/questions/19715144/how-to-convert-char-to-lpcwstr
wchar_t *convertCharArrayToLPCWSTR(const char* charArray);
//

SOCKET Listen;
typedef SOCKET Socket;


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
			while (1);
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
		printf("\nError in Startup session.\n"); WSACleanup(); return;
	};

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(PORT);

	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET)
	{
		printf("\nError in New a Socket.");
		WSACleanup();
		return;
	}

	if (bind(listen_socket, (sockaddr *)&local, sizeof(local)) != 0)
	{
		printf("\n Error in Binding socket.");
		WSACleanup();
		return;
	};

	if (listen(listen_socket, 5) != 0)
	{
		printf("\n Error in Listen."); WSACleanup(); return;
	}
	Listen = listen_socket;
	AfxBeginThread(ClientToProxy, (LPVOID)listen_socket);
}

UINT ClientToProxy(void * lParam)
{
	Socket socket = (Socket)lParam;
	SocketPair Pair;
	SOCKET SClient;
	sockaddr_in addr;
	int addr_len = sizeof(addr);
	SClient = accept(socket, (sockaddr*)&addr, &addr_len);
	AfxBeginThread(ClientToProxy, lParam);
	char Buffer[10240];
	int Len;
	Pair.IsServerClosed = FALSE;
	Pair.IsClientClosed = FALSE;
	Pair.Client = SClient;
	int retval = recv(Pair.Client, Buffer, sizeof(Buffer), 0);
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
	retval >= 10240 ? Buffer[retval - 1] = 0 : (retval > 0 ? Buffer[retval] = 0 : Buffer[0] = 0);
	cout << "\n Client received " << retval << "data :\n[" << Buffer << "]";
	string buf(Buffer), address;
	int port;
	GetAddrNPort(buf, address, port);
	Param P;
	P.handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	P.address = address;
	P.port = port;
	P.pair = &Pair;
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
		retval = recv(Pair.Client, Buffer, sizeof(Buffer), 0);
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
		retval >= 10240 ? Buffer[retval - 1] = 0 : (retval > 0 ? Buffer[retval] = 0 : Buffer[0] = 0);
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
	sockaddr_in *server;
	hostent *hp;
	cout << "Server Name: " << server_name << endl;
	if (server_name.size() > 0) {
		if (isalpha(server_name.at(0))) {
			addrinfo hints, *res = NULL, *p = NULL;
			sockaddr_in *ip_access;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			if ((status = getaddrinfo(server_name.c_str(), "80", &hints, &res)) != 0) {
				printf("getaddrinfo failed: %s", gai_strerror(status));
				return -1;
			}
			while (res->ai_next != NULL) {
				res = res->ai_next;
			}
			sockaddr_in * temp = (sockaddr_in*)res->ai_addr;
			server = (sockaddr_in*)res->ai_addr;
			server->sin_port = htons(80);
			inet_ntop(res->ai_family, &temp->sin_addr, hostname, 32);
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
				return -1;
			}
		}
	}
	if (strlen(hostname) > 0) {
		cout << "connecting to:" << hostname << endl;

		int retval;
		char Buffer[10000];
		Socket Server;
		Server = socket(AF_INET, SOCK_STREAM, 0);
		CSocket C;
		C.Create();
		if (!C.Connect(convertCharArrayToLPCWSTR(hostname),80)) {
			cout << "Khong the ket noi";
			return -1;
		}
		else {
			cout << "Ket noi thanh cong \n";
			P->pair->Server = C.Detach();
			C.Close();
			P->pair->IsServerClosed == FALSE;
			SetEvent(P->handle);
			Sleep(2000);
			while (P->pair->IsClientClosed == FALSE &&
				P->pair->IsServerClosed == FALSE) {
				retval = recv(P->pair->Server, Buffer, sizeof(Buffer), 0);
				if (retval == SOCKET_ERROR) {
					cout << "\nS Receive Failed, Error: " << GetLastError();
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
				retval>=10000? Buffer[retval-1]=0:Buffer[retval] = 0;
				cout << "\n Server received " << retval << "data :\n[" << Buffer << "]";
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
