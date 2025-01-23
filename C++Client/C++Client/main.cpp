#include <winsock2.h>
#include <windows.h>
#include <iostream>

int main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
		return;
	}

	SOCKET socket(int af, int type, int protocol);
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}
	
	int port = 654321;
	const char* host = "127.0.0.1";
	int inet_pton(int af, const char* src, void* dst);
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
		std::cerr << "Invalid address/ Address not supported" << std::endl;
		closesocket(client_socket);
		WSACleanup();
		return;
	}
}