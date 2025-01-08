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
#include <ldap.h>
#include <tuple>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>

#define BUF 1024

std::mutex loginMutex;

std::unordered_map<std::string, int> loginFailCount;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastLoginAttempt;

// Function to show the usage of the program
void showUsage(const char *programName)
{
    std::cout << "Usage: " << programName << " <port> <mail-spool-directoryname>\n";
}

// Function to get the next message ID by extracting the number from the message file name
int getNextMessageId(const std::string &userDir)
{
    int maxId = 0;
    for (const auto &entry : std::filesystem::directory_iterator(userDir))
    {
        if (entry.path().extension() == ".msg")
        {
            std::string filename = entry.path().stem().string();
            int id = std::stoi(filename);
            if (id > maxId)
            {
                maxId = id;
            }
        }
    }
    return maxId + 1;
}

std::string getClientIP(int client_socket)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        return inet_ntoa(addr.sin_addr); // Return client IP
    }
    return "unknown";
}

bool isBlacklisted(const std::string &key)
{
    loginMutex.lock();
    auto it = loginFailCount.find(key);
    if (it != loginFailCount.end())
    {
        // Check if more than 3 fails or the blacklist time has expired
        auto now = std::chrono::steady_clock::now();
        if (it->second >= 3 && now - lastLoginAttempt[key] <= std::chrono::minutes(1))
        {
            loginMutex.unlock();
            return true;
        }
        if (now - lastLoginAttempt[key] > std::chrono::minutes(1))
        {
            loginFailCount.erase(it); // Only remove expired entries
        }
    }

    loginMutex.unlock();
    return false; // Not in blacklist
}

// Function to handle the LOGIN command
void handleLogin(int client_socket, const std::string &ldap_username, const std::string &password, std::string &sessionUsername)
{
    const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
    const int ldapVersion = LDAP_VERSION3;

    // username
    char ldapBindUser[256];
    char rawLdapUser[128];
    strcpy(rawLdapUser, ldap_username.c_str());
    sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", rawLdapUser);

    std::string user_ip = getClientIP(client_socket);

    std::string ip_user_key = user_ip + "_" + ldap_username;

    // load blacklist

    // check is ip + user blacklisted?
    if (isBlacklisted(ip_user_key))
    {
        std::cerr << "user ip are blacklisted\n";
        send(client_socket, "ERR\nblacklisted\n", 16, 0);
        return;
    }

    // password
    char ldapBindPassword[256];
    strcpy(ldapBindPassword, password.c_str());

    // search settings
    const char *ldapSearchBaseDomainComponent = "dc=technikum-wien,dc=at";
    const std::string searchFilter = "(uid=" + ldap_username + ")";
    const char *ldapSearchFilter = searchFilter.c_str();
    ber_int_t ldapSearchScope = LDAP_SCOPE_SUBTREE;
    const char *ldapSearchResultAttributes[] = {"uid", NULL};

    int rc; // return code

    // Initialize LDAP connection
    LDAP *ldapHandle;
    rc = ldap_initialize(&ldapHandle, ldapUri);
    if (rc != LDAP_SUCCESS)
    {
        std::cerr << "LDAP initialization failed: " << ldap_err2string(rc) << "\n";
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    std::cout << "Connected to LDAP server at " << ldapUri << "\n";

    // set version options
    rc = ldap_set_option(
        ldapHandle,
        LDAP_OPT_PROTOCOL_VERSION, // OPTION
        &ldapVersion);             // IN-Value
    if (rc != LDAP_OPT_SUCCESS)
    {
        std::cerr << "ldap_set_option(PROTOCOL_VERSION): " << ldap_err2string(rc) << "\n";
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    // start connection secure (initialize TLS)
    rc = ldap_start_tls_s(
        ldapHandle,
        NULL,
        NULL);
    if (rc != LDAP_SUCCESS)
    {
        std::cerr << "ldap_start_tls_s(): " << ldap_err2string(rc) << "\n";
        // fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    // bind credentials
    BerValue bindCredentials;
    bindCredentials.bv_val = (char *)password.c_str();
    bindCredentials.bv_len = password.length();
    BerValue *servercredp; // server's credentials
    rc = ldap_sasl_bind_s(
        ldapHandle,
        ldapBindUser,
        LDAP_SASL_SIMPLE,
        &bindCredentials,
        NULL,
        NULL,
        &servercredp);
    if (rc != LDAP_SUCCESS)
    {
        std::cerr << "LDAP bind error: " << ldap_err2string(rc) << "\n";

        if (rc == LDAP_INVALID_CREDENTIALS)
        {
            loginMutex.lock();
            (loginFailCount[ip_user_key])++;
            lastLoginAttempt[ip_user_key] = std::chrono::steady_clock::now();

            if (loginFailCount[ip_user_key] >= 3)
            {
                std::cerr << "user ip added to blacklisted\n";
                send(client_socket, "ERR\nip and user blacklisted for 1 minute", 40, 0);
                return;
            }
            loginMutex.unlock();
        }
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    loginMutex.lock();
    auto it = loginFailCount.find(ip_user_key);
    if (it != loginFailCount.end())
    {
        loginFailCount.erase(it); // Remove entry after log in
    }
    loginMutex.unlock();

    // perform ldap search
    LDAPMessage *searchResult;
    rc = ldap_search_ext_s(
        ldapHandle,
        ldapSearchBaseDomainComponent,
        ldapSearchScope,
        ldapSearchFilter,
        (char **)ldapSearchResultAttributes,
        0,
        NULL,
        NULL,
        NULL,
        500,
        &searchResult);
    if (rc != LDAP_SUCCESS)
    {
        std::cerr << "LDAP search error: " << ldap_err2string(rc) << "\n";
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    // Get the user's DN
    LDAPMessage *entry = ldap_first_entry(ldapHandle, searchResult);
    if (!entry)
    {
        std::cerr << "User not found.\n";
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    char *dn = ldap_get_dn(ldapHandle, entry);
    if (!dn)
    {
        std::cerr << "Failed to get DN for user.\n";
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    std::cout << "User DN: " << dn << "\n";

    BerElement *ber;
    char *searchResultEntryAttribute;
    for (searchResultEntryAttribute = ldap_first_attribute(ldapHandle, entry, &ber);
         searchResultEntryAttribute != NULL;
         searchResultEntryAttribute = ldap_next_attribute(ldapHandle, entry, ber))
    {
        BerValue **vals;
        if ((vals = ldap_get_values_len(ldapHandle, entry, searchResultEntryAttribute)) != NULL)
        {
            for (int i = 0; i < ldap_count_values_len(vals); i++)
            {
                // vals[i]->bv_val is the username that needs to be stored for the session
                printf("\t%s: %s\n", searchResultEntryAttribute, vals[i]->bv_val);
                sessionUsername = vals[i]->bv_val;
            }
            ldap_value_free_len(vals);
        }

        // free memory
        ldap_memfree(searchResultEntryAttribute);
    }
    // free memory
    if (ber != NULL)
    {
        ber_free(ber, 0);
    }

    printf("\n");

    ldap_memfree(dn);           // Free DN memory after use
    ldap_msgfree(searchResult); // Free the search result
    // Clean up and unbind
    ldap_unbind_ext_s(ldapHandle, NULL, NULL);

    // Send success message
    send(client_socket, "OK\n", 3, 0);
}

// Function to handle the SEND command
void handleSend(int client_socket, const std::string &sender, const std::string &receiver, const std::string &subject, const std::string &message, const std::string &mailDir)
{
    loginMutex.lock();
    std::string userDir = mailDir + "/" + receiver;
    std::filesystem::create_directories(userDir);

    int messageId = getNextMessageId(userDir);

    std::string messageFile = userDir + "/" + std::to_string(messageId) + ".msg";
    std::ofstream outFile(messageFile);
    if (outFile)
    {
        outFile << "Sender: " << sender << "\n";
        outFile << "Subject: " << subject << "\n";
        outFile << "Message:\n"
                << message;
        outFile.close();
        send(client_socket, "OK\n", 3, 0);
    }
    else
    {
        send(client_socket, "ERR\n", 4, 0);
    }
    loginMutex.unlock();
}

// Function to handle the LIST command
void handleList(int client_socket, const std::string &user, const std::string &mailDir)
{
    loginMutex.lock();
    std::string userDir = mailDir + "/" + user;
    if (!std::filesystem::exists(userDir))
    {
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    std::string response;
    int count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(userDir))
    {
        if (entry.path().extension() == ".msg")
        {
            count++;
            response += entry.path().filename().string() + "\n";
        }
    }
    response = std::to_string(count) + ": " + "\n" + response;
    loginMutex.unlock();
    if (count == 0)
    {
        send(client_socket, "ERR\n", 4, 0);
    }
    else
    {
        send(client_socket, response.c_str(), response.size(), 0);
    }
}

// Function to handle the READ command
void handleRead(int client_socket, const std::string &username, const std::string &message_number, const std::string &mailDir)
{
    loginMutex.lock();
    std::string userDir = mailDir + "/" + username;
    std::string messageFile = userDir + "/" + message_number + ".msg";
    std::ifstream inFile(messageFile);

    if (inFile)
    {
        std::string line, response;
        while (std::getline(inFile, line))
        {
            response += line + "\n";
        }
        inFile.close();
        send(client_socket, response.c_str(), response.size(), 0);
    }
    else
    {
        send(client_socket, "ERR\n", 4, 0);
    }
    loginMutex.unlock();
}

// Function to handle the DEL command
void handleDel(int client_socket, const std::string &username, const std::string &message_number, const std::string &mailDir)
{
    loginMutex.lock();
    std::string userDir = mailDir + "/" + username;
    std::string messageFile = userDir + "/" + message_number + ".msg";

    if (std::filesystem::remove(messageFile))
    {
        send(client_socket, "OK\n", 3, 0);
    }
    else
    {
        send(client_socket, "ERR\n", 4, 0);
    }
}

// Function for the client communication where the server handles the commands
void clientCommunication(int client_socket, const std::string &mailDir)
{
    send(client_socket, "Welcome to the server!\n", 23, 0);

    char buffer[BUF];

    std::string sessionUsername = "";

    // Loop to handle the client requests
    while (true)
    {
        int size = recv(client_socket, buffer, BUF, 0);
        if (size <= 0)
            break;
        buffer[size] = '\0';
        std::istringstream request(buffer);

        std::string command, param1, param2, message;
        request >> command >> param1 >> std::ws;
        std::getline(request, param2);
        std::getline(request, message, '\0');

        if (command == "LOGIN")
        {
            if (sessionUsername != "")
            {
                send(client_socket, "ERR\nAlready logged in\n", 21, 0);
                continue;
            }
            handleLogin(client_socket, param1, param2, sessionUsername);
        }
        else if (command == "SEND")
        {
            if (sessionUsername == "")
            {
                send(client_socket, "ERR\nLogin first\n", 17, 0);
                continue;
            }
            // param1 = receiver
            // param2 = subject
            handleSend(client_socket, sessionUsername, param1, param2, message, mailDir);
        }
        else if (command == "LIST")
        {
            if (sessionUsername == "")
            {
                send(client_socket, "ERR\nLogin first\n", 17, 0);
                continue;
            }
            handleList(client_socket, sessionUsername, mailDir);
        }
        else if (command == "READ")
        {
            if (sessionUsername == "")
            {
                send(client_socket, "ERR\nLogin first\n", 17, 0);
                continue;
            }
            // param1 = message_number
            handleRead(client_socket, sessionUsername, param1, mailDir);
        }
        else if (command == "DEL")
        {
            if (sessionUsername == "")
            {
                send(client_socket, "ERR\nLogin first\n", 17, 0);
                continue;
            }
            // param1 = message_number
            handleDel(client_socket, sessionUsername, param1, mailDir);
        }
        else if (command == "QUIT")
        {
            break;
        }
    }
    close(client_socket);
}

// Main function where the server initializes and waits for client connections
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        showUsage(argv[0]);
        return EXIT_FAILURE;
    }

    int port = std::stoi(argv[1]);
    std::string ldap_username = "";
    std::string mailDir = "src/" + std::string(argv[2]);

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_port = htons(port);       // Port (im Netzwerk-Byte-Order)
    server_addr.sin_addr.s_addr = INADDR_ANY; // Akzeptiere Verbindungen von jeder Adresse

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0 || bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0 || listen(server_socket, 5) < 0)
    {
        perror("Error initializing server");
        return EXIT_FAILURE;
    }
    
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr *)&client_addr, &client_len);
        if (client_socket >= 0)
        {
            std::thread newThread(clientCommunication, client_socket, mailDir);
            newThread.detach();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    close(server_socket);
    return EXIT_SUCCESS;
}