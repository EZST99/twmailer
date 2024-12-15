// client.cpp
#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits>

#define BUF 1024

void showUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <ip> <port>\n"
              << "IP and port define the address of the server application.\n";
}

void sendRequest(int client_socket, const std::string &request) {
    if (send(client_socket, request.c_str(), request.size(), 0) == -1) {
        perror("Send error");
        exit(EXIT_FAILURE);
    }
}

std::string receiveResponse(int client_socket) {
    std::string buffer(BUF, '\0');
    int size = recv(client_socket, &buffer[0], buffer.size(), 0);
    if (size == -1) {
        perror("Receive error");
        exit(EXIT_FAILURE);
    } else if (size == 0) {
        std::cerr << "Server closed the connection.\n";
        exit(EXIT_FAILURE);
    }
    buffer.resize(size);
    return buffer;
}

void handleSend(int client_socket) {
    std::string sender, receiver, subject, message, line;

    std::cout << "Sender: ";
    std::cin >> sender;
    std::cout << "Receiver: ";
    std::cin >> receiver;
    std::cin.ignore(); // Clear input buffer
    std::cout << "Subject: ";
    std::getline(std::cin, subject);
    std::cout << "Message (end with a single '.' on a new line):\n";
    while (std::getline(std::cin, line) && line != ".") {
        message += line + "\n";
    }

    std::string request = "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n" + message + ".\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n" << response << std::endl;
}

void handleList(int client_socket) {
    std::string user;
    std::cout << "User: ";
    std::cin >> user;

    std::string request = "LIST\n" + user + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);

    if (response == "ERR\n") {
        std::cout << "No messages for user: " << user << std::endl;
    } else {
        std::cout << "Server: \n" << response << std::endl;
    }

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void handleRead(int client_socket) {
    std::string username, message_number;

    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Message Number: ";
    std::cin >> message_number;
    std::cin.ignore();

    std::string request = "READ\n" + username + "\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n" << response << std::endl;
}

void handleDel(int client_socket) {
    std::string username, message_number;
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Message number: ";
    std::cin >> message_number;
    std::cin.ignore();
    std::string request = "DEL\n" + username + "\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n" << response << std::endl;
}

void handleQuit(int client_socket) {
    sendRequest(client_socket, "QUIT\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        showUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* serverIp = argv[1];
    int serverPort = std::stoi(argv[2]);

    int client_socket;
    sockaddr_in server_address;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(serverPort);
    if (!inet_aton(serverIp, &server_address.sin_addr)) {
        std::cerr << "Invalid IP address format.\n";
        return EXIT_FAILURE;
    }

    if (connect(client_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Connection error");
        return EXIT_FAILURE;
    }

    std::cout << "Connection with server established!\n";

    std::string mainBuffer(BUF, '\0');
    int size = recv(client_socket, &mainBuffer[0], mainBuffer.size(), 0);
    if (size > 0) {
        mainBuffer.resize(size);
        std::cout << mainBuffer;
    }

    while (true) {
        std::cout << ">> ";
        std::string command;
        std::getline(std::cin, command);

        if (command.empty())
            continue;

        if (command == "SEND") {
            handleSend(client_socket);
        } else if (command == "LIST") {
            handleList(client_socket);
        } else if (command == "READ") {
            handleRead(client_socket);
        } else if (command == "DEL") {
            handleDel(client_socket);
        } else if (command == "QUIT") {
            handleQuit(client_socket);
            break;
        } else {
            std::cout << "Unknown command. Try SEND, LIST, READ, DEL, or QUIT.\n";
        }
    }

    close(client_socket);
    return EXIT_SUCCESS;
}

