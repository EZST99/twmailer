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
#define PORT 6543

int abortRequested = 0;
int create_socket = -1;

void signalHandler(int sig);
void handleSend(int current_socket, const std::string& sender, const std::string& receiver, const std::string& subject, const std::string& message);
void handleList(int current_socket, const std::string& user);

void clientCommunication(int current_socket) {
    std::string buffer(BUF, '\0');

    std::string welcomeMessage = "Welcome to myserver!\r\nPlease enter your commands...\r\n";
    send(current_socket, welcomeMessage.c_str(), welcomeMessage.length(), 0);

    do {
        int size = recv(current_socket, &buffer[0], buffer.size(), 0);
        if (size > 0) {
            buffer.resize(size); // Puffergröße anpassen
            std::istringstream request(buffer);
            std::string command;
            request >> command; // Lese den Befehl aus

            if (command == "SEND") {
                std::string sender, receiver, subject, message, line;
                request >> sender >> receiver;
                std::getline(request, subject);
                while (std::getline(request, line) && line != ".") {
                    message += line + "\n";
                }
                handleSend(current_socket, sender, receiver, subject, message);
            }
            else if (command == "LIST") {
                std::string user;
                request >> user;
                handleList(current_socket, user);
            }
            else {
                send(current_socket, "ERR\n", 4, 0);
            }
        } else if (size == 0) {
            std::cout << "Client closed the connection.\n";
            break;
        } else {
            perror("Recv error");
            break;
        }
    } while (std::string(buffer) != "quit" && !abortRequested);

    shutdown(current_socket, SHUT_RDWR);
    close(current_socket);
}

int main() {
    sockaddr_in address, cliaddress;
    socklen_t addrlen;
    int reuseValue = 1;

    signal(SIGINT, signalHandler);

    // Socket erstellen
    if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    setsockopt(create_socket, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(reuseValue));
    setsockopt(create_socket, SOL_SOCKET, SO_REUSEPORT, &reuseValue, sizeof(reuseValue));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(create_socket, (sockaddr*)&address, sizeof(address)) == -1) {
        perror("Bind error");
        return EXIT_FAILURE;
    }

    if (listen(create_socket, 5) == -1) {
        perror("Listen error");
        return EXIT_FAILURE;
    }

    while (!abortRequested) {
        std::cout << "Waiting for connections...\n";
        addrlen = sizeof(cliaddress);
        int new_socket = accept(create_socket, (sockaddr*)&cliaddress, &addrlen);
        if (new_socket == -1) {
            if (abortRequested) {
                perror("Accept error after abort");
            } else {
                perror("Accept error");
            }
            break;
        }

        std::cout << "Client connected from " << inet_ntoa(cliaddress.sin_addr) 
                  << ":" << ntohs(cliaddress.sin_port) << "\n";
        clientCommunication(new_socket);
    }

    if (create_socket != -1) {
        shutdown(create_socket, SHUT_RDWR);
        close(create_socket);
    }

    return EXIT_SUCCESS;
}

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "Abort requested...\n";
        abortRequested = 1;
        close(create_socket);
    }
}

void handleSend(int client_socket, const std::string& sender, const std::string& receiver, const std::string& subject, const std::string& message) {
    std::string userDir = "./mail-spool/" + receiver;
    std::filesystem::create_directories(userDir);

    int messageId = 1;
    for (const auto& entry : std::filesystem::directory_iterator(userDir)) {
        messageId++;
    }

    std::string messageFile = userDir + "/" + std::to_string(messageId) + ".msg";
    std::ofstream outFile(messageFile);
    if (outFile) {
        outFile << "Sender: " << sender << "\n";
        outFile << "Subject: " << subject << "\n";
        outFile << "Message:\n" << message << "\n";
        outFile.close();
        send(client_socket, "OK\n", 3, 0);
    } else {
        send(client_socket, "ERR\n", 4, 0);
    }
}

void handleList(int client_socket, const std::string& user) {
    std::string userDir = "./mail-spool/" + user;

    if (!std::filesystem::exists(userDir)) {
        send(client_socket, "0\n", 2, 0); 
        return;
    }

    std::string response;
    int count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(userDir)) {
        if (entry.path().extension() == ".msg") {
            count++;
            std::ifstream inFile(entry.path());
            if (inFile) {
                std::string line;
                std::getline(inFile, line); 
                if (line.rfind("Subject: ", 0) == 0) {
                    response += std::to_string(count) + ": " + line.substr(9) + "\n"; 
                } else {
                    response += std::to_string(count) + ": (No subject)\n"; 
                }
            }
        }
    }

    // Wenn keine Nachrichten gefunden wurden
    if (count == 0) {
        send(client_socket, "0\n", 2, 0);
        return;
    }

    // Anzahl der Nachrichten an den Anfang der Antwort setzen
    response = std::to_string(count) + "\n" + response;

    // Sende die Antwort an den Client
    send(client_socket, response.c_str(), response.size(), 0);
}




