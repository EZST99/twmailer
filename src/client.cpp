#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits>
#include <termios.h>

#define BUF 1024

// Function to validate the username
bool validateUsername(const std::string &username)
{
    if (username.length() > 8)
        return false;
    for (char c : username)
    {
        if (!isalnum(c))
        {
            return false;
        }
    }
    return true;
}

// Function to validate the subject
bool validateSubject(const std::string &subject)
{
    return subject.length() <= 80;
}

// Function to show the usage of the program
void showUsage(const char *programName)
{
    std::cout << "Usage: " << programName << " <ip> <port>\n"
              << "IP and port define the address of the server application.\n";
}

// Function to send a request to the server
void sendRequest(int client_socket, const std::string &request)
{
    if (send(client_socket, request.c_str(), request.size(), 0) == -1)
    {
        perror("Send error");
        exit(EXIT_FAILURE);
    }
}

// Function to receive a response from the server
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

// Function to get password with hidden input
int getch()
{
    int ch;
    // https://man7.org/linux/man-pages/man3/termios.3.html
    struct termios t_old, t_new;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    // tcgetattr() gets the parameters associated with the object referred
    //   by fd and stores them in the termios structure referenced by
    //   termios_p
    tcgetattr(STDIN_FILENO, &t_old);

    // copy old to new to have a base for setting c_lflags
    t_new = t_old;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    //
    // ICANON Enable canonical mode (described below).
    //   * Input is made available line by line (max 4096 chars).
    //   * In noncanonical mode input is available immediately.
    //
    // ECHO   Echo input characters.
    t_new.c_lflag &= ~(ICANON | ECHO);

    // sets the attributes
    // TCSANOW: the change occurs immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    // reset stored attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);

    return ch;
}
// Function to get password with hidden input
const char *getpass()
{
    int show_asterisk = 0;

    const char BACKSPACE = 127;
    const char RETURN = 10;

    unsigned char ch = 0;
    std::string password;

    printf("Password: ");

    while ((ch = getch()) != RETURN)
    {
        if (ch == BACKSPACE)
        {
            if (password.length() != 0)
            {
                if (show_asterisk)
                {
                    printf("\b \b"); // backslash: \b
                }
                password.resize(password.length() - 1);
            }
        }
        else
        {
            password += ch;
            if (show_asterisk)
            {
                printf("*");
            }
        }
    }
    printf("\n");
    return password.c_str();
}

// Function to login with LDAP
void handleLogin(int client_socket)
{
    std::string ldap_username, password;
    std::cout << "LDAP username: ";
    std::cin >> ldap_username;
    std::cout << "Password: ";
    std::cin >> password;
    // todo implement hidden input for pw
    // password = getpass();

    std::string request = "LOGIN\n" + ldap_username + "\n" + password + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);

    if (response == "ERR\n")
    {
        std::cout << "Login failed." << std::endl;
    }
    else
    {
        std::cout << "Server: \n"
                  << response << std::endl;
    }

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Function to handle the SEND command
void handleSend(int client_socket)
{
    std::string receiver, subject, message, line;

    while (true)
    {
        std::cout << "Receiver: ";
        std::cin >> receiver;
        if (validateUsername(receiver))
        {
            break;
        }
        else
        {
            std::cout << "Invalid username. Must be alphanumeric and at most 8 characters long.\n";
        }
    }
    std::cin.ignore(); // Clear input buffer
    while (true)
    {
        std::cout << "Subject: ";
        std::getline(std::cin, subject);
        if (validateSubject(subject))
        {
            break;
        }
        else
        {
            std::cout << "Invalid subject. Must be at most 80 characters long.\n";
        }
    }
    std::cout << "Message (end with a single '.' on a new line):\n";
    while (std::getline(std::cin, line) && line != ".")
    {
        message += line + "\n";
    }

    std::string request = "SEND\n" + receiver + "\n" + subject + "\n" + message + ".\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n"
              << response << std::endl;
}

// Function to handle the LIST command
void handleList(int client_socket)
{

    std::string request = "LIST\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);

    if (response == "ERR\n")
    {
        std::cout << "No messages available" << std::endl;
    }
    else
    {
        std::cout << "Server: \n"
                  << response << std::endl;
    }

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Function to handle the READ command
void handleRead(int client_socket)
{
    std::string message_number;

    std::cout << "Message Number: ";
    std::cin >> message_number;
    std::cin.ignore();

    std::string request = "READ\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "\n"
              << "Server: \n"
              << response << std::endl;
}

// Function to handle the DEL command
void handleDel(int client_socket)
{
    std::string message_number;

    std::cout << "Message number: ";
    std::cin >> message_number;
    std::cin.ignore();
    std::string request = "DEL\n" + message_number + "\n";
    sendRequest(client_socket, request);

    std::string response = receiveResponse(client_socket);
    std::cout << "Server: \n"
              << response << std::endl;
}

// Function to handle the QUIT command
void handleQuit(int client_socket)
{
    sendRequest(client_socket, "QUIT\n");
}

// Main function where the client connects to the server and waits for user input
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        showUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *serverIp = argv[1];
    int serverPort = std::stoi(argv[2]);

    int client_socket;
    sockaddr_in server_address;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(serverPort);
    if (!inet_aton(serverIp, &server_address.sin_addr))
    {
        std::cerr << "Invalid IP address format.\n";
        return EXIT_FAILURE;
    }

    if (connect(client_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Connection error");
        return EXIT_FAILURE;
    }

    std::cout << "Connection with server established!\n";

    std::string mainBuffer(BUF, '\0');
    int size = recv(client_socket, &mainBuffer[0], mainBuffer.size(), 0);
    if (size > 0)
    {
        mainBuffer.resize(size);
        std::cout << mainBuffer;
    }

    // Main loop to handle user input
    while (true)
    {
        std::cout << ">> ";
        std::string command;
        std::getline(std::cin, command);

        if (command.empty())
            continue;

        if (command == "LOGIN")
        {
            handleLogin(client_socket);
        }
        else if (command == "SEND")
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
            std::cout << "Unknown command. Try LOGIN, SEND, LIST, READ, DEL, or QUIT.\n";
        }
    }

    close(client_socket);
    return EXIT_SUCCESS;
}
