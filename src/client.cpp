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

// Function to validate the username
bool validateUsername(const std::string &username) {
    if(username.length() > 8)
        return false;
    for (char c : username) {
        if (!isalnum(c)) {
            return false;
        }
    }
    return true;
}

// Function to validate the subject
bool validateSubject(const std::string &subject) {
    return subject.length() <= 80;
}

// Function to show the usage of the program
void showUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <ip> <port>\n"
              << "IP and port define the address of the server application.\n";
}

// Function to send a request to the server
void sendRequest(int client_socket, const std::string &request) {
    if (send(client_socket, request.c_str(), request.size(), 0) == -1) {
        perror("Send error");
        exit(EXIT_FAILURE);
    }
}

// Function to receive a response from the server
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

// Function to handle the SEND command
void handleSend(int client_socket) {
    std::string sender, receiver, subject, message, line;
    while(true) {
        std::cout << "Sender: ";
        std::cin >> sender;
        if (validateUsername(sender)) {
            break;
        } else {
            std::cout << "Invalid username. Must be alphanumeric and at most 8 characters long.\n";
        }
    }
    while(true) {
        std::cout << "Receiver: ";
        std::cin >> receiver;
        if (validateUsername(receiver)) {
            break;
        } else {
            std::cout << "Invalid username. Must be alphanumeric and at most 8 characters long.\n";
        }
    }
    std::cin.ignore(); // Clear input buffer
    while(true)
    {
        std::cout << "Subject: ";
        std::getline(std::cin, subject);
        if (validateSubject(subject)) {
            break;
        } else {
            std::cout << "Invalid subject. Must be at most 80 characters long.\n";
        }
    }
    std::cout << "Message (end with a single '.' on a new line):\n";
    while (std::getline(std::cin, line) && line != ".") {
        message += line + "\n";
    }

    std::string request = "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n" + message + ".\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n" << response << std::endl;
}

// Function to handle the LIST command
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

// Function to handle the READ command
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
    std::cout << "\n" << "Server: \n" << response << std::endl;
}

// Function to handle the DEL command
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

// Function to handle the QUIT command
void handleQuit(int client_socket) {
    sendRequest(client_socket, "QUIT\n");
}

// Main function where the client connects to the server and waits for user input
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

    // Main loop to handle user input
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

