#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Buffer size definition
#define BUF 1024
#define PORT 6543

// Function declarations
void sendRequest(int client_socket, const std::string &request);
std::string receiveResponse(int client_socket);
void handleSend(int client_socket);
void handleList(int client_socket);
void handleRead(int client_socket);
void handleDel(int client_socket);
void handleQuit(int client_socket);

int main(int argc, char **argv)
{
    int client_socket;
    sockaddr_in server_address;

    // Create the socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    // Server address configuration
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (argc > 1)
    {
        inet_aton(argv[1], &server_address.sin_addr);
    }
    else
    {
        inet_aton("127.0.0.1", &server_address.sin_addr);
    }

    // Connect to the server
    if (connect(client_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Connection error");
        return EXIT_FAILURE;
    }

    std::cout << "Connection with server established!\n";

    // Buffer for receiving server messages
    std::string mainBuffer(BUF, '\0');
    int size = recv(client_socket, &mainBuffer[0], mainBuffer.size(), 0);
    if (size > 0)
    {
        mainBuffer.resize(size);
        std::cout << mainBuffer;
    }

    // Main loop for client commands
    while (true)
    {
        std::cout << ">> ";
        std::string command;
        std::getline(std::cin, command);

        if (command.empty()) // Ignoriere leere Befehle
            continue;

        if (command == "SEND")
        {
            handleSend(client_socket);
        }
        else if (command == "LIST")
        {
            handleList(client_socket);
        }
        else if (command == "READ")
        {
            handleRead(client_socket);
        }
        else if (command == "DEL")
        {
            handleDel(client_socket);
        }
        else if (command == "QUIT")
        {
            handleQuit(client_socket);
            break;
        }
        else
        {
            std::cout << "Unknown command. Try SEND, LIST, READ, DEL, or QUIT.\n";
        }
    }

    // Close the socket
    close(client_socket);
    return EXIT_SUCCESS;
}

// Function definitions

void sendRequest(int client_socket, const std::string &request)
{
    if (send(client_socket, request.c_str(), request.size(), 0) == -1)
    {
        perror("Send error");
        exit(EXIT_FAILURE);
    }
}

std::string receiveResponse(int client_socket)
{
    std::string buffer(BUF, '\0');
    int size = recv(client_socket, &buffer[0], buffer.size(), 0);
    if (size == -1)
    {
        perror("Receive error");
        exit(EXIT_FAILURE);
    }
    else if (size == 0)
    {
        std::cerr << "Server closed the connection.\n";
        exit(EXIT_FAILURE);
    }
    buffer.resize(size);
    return buffer;
}

void handleSend(int client_socket)
{
    std::string sender, receiver, subject, message, line;

    std::cout << "Sender: ";
    std::cin >> sender;
    std::cout << "Receiver: ";
    std::cin >> receiver;
    std::cin.ignore(); // Clear input buffer
    std::cout << "Subject: ";
    std::getline(std::cin, subject);
    std::cout << "Message (end with a single '.' on a new line):\n";
    while (std::getline(std::cin, line) && line != ".")
    {
        message += line + "\n";
    }

    std::string request = "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n" + message + ".\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: " << response << std::endl;
}

void handleList(int client_socket)
{
    std::string user;
    std::cout << "User: ";
    std::cin >> user;

    std::string request = "LIST\n" + user + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);

    if (response == "ERR\n") {
        std::cout << "No messages for user: " << user << std::endl;
    } else {
        std::cout << "Server:\n" << response;
    }
}

void handleRead(int client_socket)
{
    std::string username, message_number;

    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Message Number: ";
    std::cin >> message_number;
    std::cin.ignore();

    std::string request = "READ\n" + username + "\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: " << response << std::endl;
}

void handleDel(int client_socket)
{
    std::string username, message_number;
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Message number: ";
    std::cin >> message_number;
    std::cin.ignore();
    std::string request = "DEL\n" + username + "\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: " << response << std::endl;
}

void handleQuit(int client_socket)
{
    sendRequest(client_socket, "QUIT\n");
}