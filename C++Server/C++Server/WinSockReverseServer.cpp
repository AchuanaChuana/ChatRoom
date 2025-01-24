#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

static std::map<SOCKET, std::string> g_clients;  
static std::mutex g_clientsMutex;

//send to a single user
void sendToSocket(SOCKET s, const std::string& msg) {
    send(s, msg.c_str(), static_cast<int>(msg.size()), 0);
}

//send to all users
void broadcastMessage(const std::string& msg) {     
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    for (auto& kv : g_clients) {
        sendToSocket(kv.first, msg);
    }
}

//DM : send a message to a specific user
void sendPrivateMessage(const std::string& toUser, const std::string& msg) {  
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    for (auto& kv : g_clients) {
        if (kv.second == toUser) {
            sendToSocket(kv.first, msg);
            break; 
        }
    }
}

//distinguish the user name and content     e.g.: [NAME]Alice -> prefix=[NAME], content=Alice
bool parseMessage(const std::string& input, std::string& prefix, std::string& content) { 
    //  [xxxx]yyyy
    if (input.size() < 2 || input[0] != '[') return false;
    auto pos = input.find(']');
    if (pos == std::string::npos) return false;

    prefix = input.substr(0, pos + 1); // [xxxx]
    content = input.substr(pos + 1);   // yyyy
    return true;
}

//get the client name
std::string getClientName(SOCKET s) { 
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    auto it = g_clients.find(s);
    if (it != g_clients.end()) {
        return it->second;
    }
    return "";
}

//remove client 
void removeClient(SOCKET s) {  
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    g_clients.erase(s);
}

void handle_client(SOCKET client_socket, int id) {
    char buffer[1024] = { 0 };
    bool stop = false;

    while (!stop) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "Received(" << id <<"):" << buffer << std::endl;

            std::string clientMsg(buffer);
            std::cout << "[DEBUG] Received(" << id << "): " << clientMsg << std::endl;

            //Exit chatroom by saying "bye"
            if (clientMsg == "bye") {
                stop = true;
                continue;
            }

            std::string prefix, content;
            if (parseMessage(clientMsg, prefix, content)) {
                if (prefix == "[NAME]") {
                    // Client tell Server : My name is content
                    {   
                        std::lock_guard<std::mutex> lock(g_clientsMutex);
                        g_clients[client_socket] = content; // update Name
                    }
                    // Broadcast "Someone Joined"
                    std::string joinMsg = "[SERVER]" + content + " has joined the chat.\n";
                    broadcastMessage(joinMsg);
                }

                else if (prefix == "[BROADCAST]") {
                    // Broadcasting
                    std::string senderName = getClientName(client_socket);
                    // [BROADCAST]Hello   ->  "Alice: Hello"
                    std::string finalMsg = senderName + ": " + content + "\n";
                    broadcastMessage(finalMsg);

                }

                else if (prefix.rfind("[DM:", 0) == 0) {
                    // DM:  [DM:Bob]Hello Bob
                    // prefix = "[DM:Bob]", content="Hello Bob"
                 
                    std::string targetName = prefix.substr(4, prefix.size() - 5);
                    std::string senderName = getClientName(client_socket);
                    std::string finalMsg = "[PM from " + senderName + "] " + content + "\n";
                    sendPrivateMessage(targetName, finalMsg);
                }

                else {
                    // Wrong prefix
                    std::string errMsg = "[SERVER] Unrecognized prefix.\n";
                    sendToSocket(client_socket, errMsg);
                }
            }

            else {
                // Distinguish failed
                std::string errMsg = "[SERVER] Invalid message format.\n";
                sendToSocket(client_socket, errMsg);
            }
        }

        else if (bytes_received == 0) {
            stop = true;
        }

        else {
            stop = true;
        }
    }

    // Exit Loop, the Client is disconnecting
    std::string leavingUser = getClientName(client_socket);
    if (!leavingUser.empty()) {
        // Broadcasting User leaving
        std::string leaveMsg = "[SERVER]" + leavingUser + " has left.\n";
        broadcastMessage(leaveMsg);
    }

    //remove client and close
    removeClient(client_socket);
    closesocket(client_socket);
}



int server() {
    // Step 1: Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

//    // Step 5: Accept a connection
//    sockaddr_in client_address = {};
//    int client_address_len = sizeof(client_address);
//    SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
//    if (client_socket == INVALID_SOCKET) {
//        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    char client_ip[INET_ADDRSTRLEN];
//    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
//    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
//
//
//    bool stop = false;
//    while (!stop) {
//        // Step 6: Communicate with the client
//        char buffer[1024] = { 0 };
//        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
//        if (bytes_received > 0) {
//            buffer[bytes_received] = '\0';
//            std::cout << "Received: " << buffer << std::endl;
//
//            // Reverse the string
//            std::string response(buffer);
//            if (response == "bye") break;
//            std::reverse(response.begin(), response.end());
//
//
//            // Send the reversed string back
//            send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
//            std::cout << "Reversed string sent back to client." << std::endl;
//        }
//    }
//
//    // Step 7: Clean up
//    closesocket(client_socket);
//    closesocket(server_socket);
//    WSACleanup();
//
//    return 0;
//}

    //std::vector<std::thread*> threads;
    //int connection = 0;
    //while (true) {
    //    // Step 5: Accept a connection
    //    sockaddr_in client_address = {};
    //    int client_address_len = sizeof(client_address);
    //    SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
    //    if (client_socket == INVALID_SOCKET) {
    //        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
    //        closesocket(server_socket);
    //        WSACleanup();
    //        return 1;
    //    }

    //    char client_ip[INET_ADDRSTRLEN];
    //    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    //    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;

    //    // add a new thread for a new client
    //    std::thread* client_thread = new std::thread(handle_client, client_socket,connection++);
    //    threads.push_back(client_thread);
    //}

    //// clean threads
    //for (auto thread : threads) {
    //    if (thread->joinable()) {
    //        thread->join();
    //    }
    //    delete thread;
    //}

    std::vector<std::thread*> threads;
    int connection = 0;
    while (true) {
        // Step 5: Accept a connection
        sockaddr_in client_address = {};
        int client_address_len = sizeof(client_address);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed.\n";
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Accepted connection #" << connection << " from "<< client_ip << ":" << ntohs(client_address.sin_port) << std::endl;

        {
            std::lock_guard<std::mutex> lock(g_clientsMutex);
            g_clients[client_socket] = "Unknown"; // Default Name
        }

        //add a new thread for new client
        std::thread* client_thread = new std::thread(handle_client, client_socket, connection++);
        threads.push_back(client_thread);
    }

    //delete thead after use
    for (auto thread : threads) {
        if (thread->joinable()) {
            thread->join();
        }
        delete thread;
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}

int main() {
    server(); 
}