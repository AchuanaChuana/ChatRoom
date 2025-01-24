#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>


#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_BUFFER_SIZE 1024

std::string buildProtocolMessage(const std::string& userInput) {
    // b:Hello everyone   -> [BROADCAST]Hello everyone
    // d:Bob:Hi Bob!      -> [DM:Bob]Hi Bob!

    if (userInput.size() < 2) {
        return userInput; 
    }

    // If the message start with "b:"
    if (userInput.rfind("b:", 0) == 0) {
        // remove "b:"
        std::string content = userInput.substr(2);
        return "[BROADCAST]" + content;
    }

    // If the message start with "d:username:"
    if (userInput.rfind("d:", 0) == 0) {
        // find second ":"
        size_t pos = userInput.find(':', 2);
        if (pos != std::string::npos) {
            // remove "d:username:"
            std::string toUser = userInput.substr(2, pos - 2);
            std::string content = userInput.substr(pos + 1);
            return "[DM:" + toUser + "]" + content;
        }
    }

    return userInput;
}

//void run_client(const char* host, unsigned int port, const std::string& sentence) {
void run_client() {
    const char* host = "127.0.0.1"; // Server IP address
    unsigned int port = 65432;
    std::string sentence = "Hello, server!";

    // Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return;
    }

    // Create a socket
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    // Resolve the server address and port
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    //server_address.sin_port = htons(std::stoi(port));
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    // Connect to the server
    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    std::cout << "Connected to the server." << std::endl;

    //Enter user name and send to server
    std::string myName;
    std::cout << "Enter your username: ";
    std::getline(std::cin, myName);
    if (myName.empty()) {
        myName = "Guest";
    }

    //  e.g. "[NAME]Alice"
    std::string nameMsg = "[NAME]" + myName;
    send(client_socket, nameMsg.c_str(), static_cast<int>(nameMsg.size()), 0);
     
    while (true) {
        std::cout << "Enter message (b:xxx for broadcast, d:User:xxx for DM, bye to exit): ";
        std::string userInput;
        std::getline(std::cin, userInput);

        if (userInput == "bye") {
            // tell server the client is leaving
            send(client_socket, userInput.c_str(), static_cast<int>(userInput.size()), 0);
            break;
        }

        std::string finalMsg = buildProtocolMessage(userInput);

        if (send(client_socket, finalMsg.c_str(), static_cast<int>(finalMsg.size()), 0) == SOCKET_ERROR) {
            std::cerr << "Send failed.\n";
            break;
        }

        // 等待服务器的返回（若服务器要回显/转发给自己）
        // 也可用一个单独的线程持续 recv，以便实时显示消息
        // 目前为了简单，先阻塞性地 recv 一次，也可以不做
        char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "Server says: " << buffer << std::endl;
        }
        // 如果 bytes_received == 0 或负数，说明服务器断开/出错，这里就可以 break
        else if (bytes_received <= 0) {
            std::cout << "Server closed or error.\n";
            break;
        }
    }

    // Cleanup
    closesocket(client_socket);
    WSACleanup();
    }


int main() {
    run_client(); 
}