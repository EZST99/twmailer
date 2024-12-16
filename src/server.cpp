#include <iostream>
#include <cstring>
#include <sstream>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>

#define BUF 1024

// Function to show the usage of the program
void showUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <port> <mail-spool-directoryname>\n";
}

// Function to get the next message ID by extracting the number from the message file name
int getNextMessageId(const std::string &userDir) {
    int maxId = 0;
    for (const auto &entry : std::filesystem::directory_iterator(userDir)) {
        if (entry.path().extension() == ".msg") {
            std::string filename = entry.path().stem().string();
            int id = std::stoi(filename);
            if (id > maxId) {
                maxId = id;
            }
        }
    }
    return maxId + 1;
}

// Function to handle the SEND command
void handleSend(int client_socket, const std::string &sender, const std::string &receiver, const std::string &subject, const std::string &message, const std::string &mailDir) {
    std::string userDir = mailDir + "/" + receiver;
    std::filesystem::create_directories(userDir);

    int messageId = getNextMessageId(userDir);

    std::string messageFile = userDir + "/" + std::to_string(messageId) + ".msg";
    std::ofstream outFile(messageFile);
    if (outFile) {
        outFile << "Sender: " << sender << "\n";
        outFile << "Subject: " << subject << "\n";
        outFile << "Message:\n" << message;
        outFile.close();
        send(client_socket, "OK\n", 3, 0);
    } else {
        send(client_socket, "ERR\n", 4, 0);
    }
}

// Function to handle the LIST command
void handleList(int client_socket, const std::string& user, const std::string &mailDir) {
    std::string userDir = mailDir + "/" + user;
    if (!std::filesystem::exists(userDir)) {
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    std::string response;
    int count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(userDir)) {
        if (entry.path().extension() == ".msg") {
            count++;
            response += entry.path().filename().string() + "\n";
        }
    }
    response = std::to_string(count) + ": " +  "\n" + response;
    if (count == 0) {
        send(client_socket, "ERR\n", 4, 0);
    } else {
        send(client_socket, response.c_str(), response.size(), 0);
    }
}

// Function to handle the READ command
void handleRead(int client_socket, const std::string &username, const std::string &message_number, const std::string &mailDir) {
    std::string userDir = mailDir + "/" + username;
    std::string messageFile = userDir + "/" + message_number + ".msg";
    std::ifstream inFile(messageFile);

    if (inFile) {
        std::string line, response;
        while (std::getline(inFile, line)) {
            response += line + "\n";
        }
        inFile.close();
        send(client_socket, response.c_str(), response.size(), 0);
    } else {
        send(client_socket, "ERR\n", 4, 0);
    }
}

// Function to handle the DEL command
void handleDel(int client_socket, const std::string &username, const std::string &message_number, const std::string &mailDir) {
    std::string userDir = mailDir + "/" + username;
    std::string messageFile = userDir + "/" + message_number + ".msg";

    if (std::filesystem::remove(messageFile)) {
        send(client_socket, "OK\n", 3, 0);
    } else {
        send(client_socket, "ERR\n", 4, 0);
    }
}

// Function for the client communication where the server handles the commands
void clientCommunication(int client_socket, const std::string &mailDir) {
    send(client_socket, "Welcome to the server!\n", 23, 0);

    char buffer[BUF];
    // Loop to handle the client requests
    while (true) {
        int size = recv(client_socket, buffer, BUF, 0);
        if (size <= 0) break;
        buffer[size] = '\0';
        std::istringstream request(buffer);

        std::string command, param1, param2, param3, message;
        request >> command >> param1 >> param2 >> std::ws;
        std::getline(request, param3);
        std::getline(request, message, '\0');


        if (command == "SEND") {
            handleSend(client_socket, param1, param2, param3, message, mailDir);
        } else if (command == "LIST") {
            handleList(client_socket, param1, mailDir);
        } else if (command == "READ") {
            handleRead(client_socket, param1, param2, mailDir);
        } else if (command == "DEL") {
            handleDel(client_socket, param1, param2, mailDir);
        } else if (command == "QUIT") {
            break;
        }
    }
    close(client_socket);
}

// Main function where the server initializes and waits for client connections
int main(int argc, char** argv) {
    if (argc != 3) {
        showUsage(argv[0]);
        return EXIT_FAILURE;
    }

    int port = std::stoi(argv[1]);
    std::string mailDir = "src/" + std::string(argv[2]);

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;           // IPv4
    server_addr.sin_port = htons(port);        // Port (im Netzwerk-Byte-Order)
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Akzeptiere Verbindungen von jeder Adresse


    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0 || bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0 || listen(server_socket, 5) < 0) {
        perror("Error initializing server");
        return EXIT_FAILURE;
    }

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        if (client_socket >= 0) {
            clientCommunication(client_socket, mailDir);
        }
    }

    close(server_socket);
    return EXIT_SUCCESS;
}