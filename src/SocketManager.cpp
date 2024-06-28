#include "SocketManager.h"
#include <cstring>
#include <fcntl.h>
#include <vector>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

SocketManager::SocketManager() : ServerFD(-1), ClientFD(-1), AddrInfo(nullptr)
{
}

SocketManager::~SocketManager()
{
    if (ServerFD != -1)
    {
        close(ServerFD);
    }
    if (ClientFD != -1)
    {
        close(ClientFD);
    }
    FreeResources();
}

void SocketManager::FreeResources()
{
    if (AddrInfo != nullptr)
    {
        freeaddrinfo(AddrInfo);
        AddrInfo = nullptr;
    }
}

bool SocketManager::ResolveAndConnect(const string& hostname, const string& port)
{
    struct addrinfo hints;
    int status;
    char ipString[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // Use IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

    FreeResources();
    if ((status = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &AddrInfo)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(status) << endl;
        return false;
    }

    for (struct addrinfo* p = AddrInfo; p != NULL; p = p->ai_next)
    {
        void* address;
        string ipVersion;

        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            address = &(ipv4->sin_addr);
            ipVersion = "IPv4";
        }
        else
        {
            continue; // Skip non-IPv4 addresses
        }

        inet_ntop(p->ai_family, address, ipString, sizeof(ipString));
        cout << "  " << ipVersion << ": " << ipString << endl;

        ClientFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (ClientFD == -1)
        {
            perror("socket");
            continue; // Try the next address in case of error
        }

        if (connect(ClientFD, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(ClientFD);
            ClientFD = -1;
            perror("connect");
            continue; // Try the next address in case of error
        }

        cout << "Successfully connected to " << hostname << " on port " << port << " (" << ipVersion << ")\n";
        return true;
    }

    cerr << "Failed to connect to any address.\n";
    return false;
}

bool SocketManager::SetupServer(const string& port)
{
    struct addrinfo hints;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;      // For wildcard IP address

    FreeResources();
    if ((status = getaddrinfo(NULL, port.c_str(), &hints, &AddrInfo)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(status) << endl;
        return false;
    }

    ServerFD = socket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol);
    if (ServerFD == -1)
    {
        perror("socket");
        return false;
    }

    int yes = 1;
    if (setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("setsockopt");
        close(ServerFD);
        ServerFD = -1;
        return false;
    }

    if (bind(ServerFD, AddrInfo->ai_addr, AddrInfo->ai_addrlen) == -1)
    {
        close(ServerFD);
        perror("bind");
        ServerFD = -1;
        return false;
    }

    if (listen(ServerFD, 10) == -1)
    {
        close(ServerFD);
        perror("listen");
        ServerFD = -1;
        return false;
    }

    cout << "Server is listening on port " << port << "\n";
    return true;
}

int SocketManager::AcceptConnection()
{
    struct sockaddr_storage theirAddr;
    socklen_t addrSize = sizeof(theirAddr);
    int newFD = accept(ServerFD, (struct sockaddr*)&theirAddr, &addrSize);
    if (newFD == -1 && ServerFD != -1)
    {
        perror("accept");
        return -1;
    }
    return newFD;
}

bool SocketManager::SendMessage(int socketFD, const string& message)
{
    uint32_t messageLength = message.length();
    uint32_t netMessageLength = htonl(messageLength); // Convert to network byte order

    size_t totalSent = 0;
    const char* lengthPtr = reinterpret_cast<const char*>(&netMessageLength);
    while (totalSent < sizeof(netMessageLength)) // Send the length of the data
    {
        ssize_t sent = send(socketFD, lengthPtr + totalSent, sizeof(netMessageLength) - totalSent, 0);
        if (sent == -1)
        {
            perror("send length");
            return false;
        }
        totalSent += sent;
    }
    
    totalSent = 0;
    const char* messagePtr = message.c_str();
    while (totalSent < messageLength) // Send the actual data
    {
        ssize_t sent = send(socketFD, messagePtr + totalSent, messageLength - totalSent, 0);
        if (sent == -1) {
            perror("send message");
            return false;
        }
        totalSent += sent;
    }

    return true;
}

bool SocketManager::ReceiveMessage(int socketFD, string& message)
{
    uint32_t netMessageLength;
    size_t totalReceived = 0;
    char* lengthPtr = reinterpret_cast<char*>(&netMessageLength);
    while (totalReceived < sizeof(netMessageLength)) // Receive the length of the message
    {
        ssize_t received = recv(socketFD, lengthPtr + totalReceived, sizeof(netMessageLength) - totalReceived, 0);
        if (received == -1)
        {
            perror("recv length");
            return false;
        }
        else if (received == 0) // Connection closed
        {
            return false;
        }
        totalReceived += received;
    }

    uint32_t messageLength = ntohl(netMessageLength); // Convert from network byte order
    vector<char> buffer(messageLength);
    totalReceived = 0;
    char* messagePtr = buffer.data();
    while (totalReceived < messageLength) // Receive the actual message
    {
        ssize_t received = recv(socketFD, messagePtr + totalReceived, messageLength - totalReceived, 0);
        if (received == -1)
        {
            perror("recv message");
            return false;
        }
        else if (received == 0)
        {
            return false;
        }
        totalReceived += received;
    }

    message.assign(buffer.begin(), buffer.end());
    return true;
}

bool SocketManager::SendFileData(int socketFD, const string& fileName)
{
    ifstream file(fileName, ios::binary | ios::ate);
    if (!file)
    {
        cerr << "Error opening file: " << fileName << endl;
        return false;
    }

    uint64_t fileSize = file.tellg();
    file.seekg(0, ios::beg);

    uint64_t netFileSize = Htonll(fileSize); // Convert to network byte order
    size_t totalSent = 0;
    const char* sizePtr = reinterpret_cast<const char*>(&netFileSize);

    while (totalSent < sizeof(netFileSize)) // Send the length of the data
    {
        ssize_t sent = send(socketFD, sizePtr + totalSent, sizeof(netFileSize) - totalSent, 0);
        if (sent == -1)
        {
            perror("send length");
            return false;
        }
        totalSent += sent;
    }

    char buffer[4096];
    while (!file.eof())
    {
        file.read(buffer, sizeof(buffer));
        streamsize bytesRead = file.gcount();
        totalSent = 0;
        const char* bufferPtr = buffer;

        while ((int)totalSent < bytesRead) // Send the actual data
        {
            ssize_t sent = send(socketFD, bufferPtr + totalSent, bytesRead - totalSent, 0);
            if (sent == -1)
            {
                perror("send message");
                return false;
            }
            totalSent += sent;
        }
    }

    file.close();
    return true;
}

bool SocketManager::ReceiveFileData(int socketFD)
{
    uint64_t netFileSize;
    size_t totalReceived = 0;
    char* sizePtr = reinterpret_cast<char*>(&netFileSize);

    while (totalReceived < sizeof(netFileSize))
    {
        ssize_t received = recv(socketFD, sizePtr + totalReceived, sizeof(netFileSize) - totalReceived, 0);
        if (received == -1) {
            perror("recv length");
            return false;
        }
        totalReceived += received;
    }

    uint64_t fileSize = Ntohll(netFileSize); // Convert from network byte order

    char buffer[4096];
    uint64_t remaining = fileSize;

    while (remaining > 0)
    {
        ssize_t toReceive = min(sizeof(buffer), static_cast<size_t>(remaining));
        ssize_t received = recv(socketFD, buffer, toReceive, 0);
        if (received == -1) {
            perror("recv message");
            return false;
        }

        cout.write(buffer, received); // Print the received chunk to the output
        remaining -= received;
    }

    return true;
}

// Converts uint64_t to network byte order (big endian)
uint64_t SocketManager::Htonll(uint64_t value)
{
    static const int num = 100; // Random number to check endianness
    if (*reinterpret_cast<const char*>(&num) == num) // Little-endian system
    {
        uint32_t high_part = htonl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = htonl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<uint64_t>(low_part) << 32) | high_part;
    }
    else // Big-endian system
    {
        return value;
    }
}

uint64_t SocketManager::Ntohll(uint64_t value)
{
    static const int num = 100;
    if (*reinterpret_cast<const char*>(&num) == num) // Little-endian system
    {
        uint32_t high_part = ntohl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = ntohl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<uint64_t>(low_part) << 32) | high_part;
    }
    else // Big-endian system
    {
        return value;
    }
}

int SocketManager::GetClientSocketFD() const
{
    return ClientFD;
}

void SocketManager::CloseServerSocket()
{
    if (ServerFD != -1)
    {
        shutdown(ServerFD, SHUT_RDWR);
        close(ServerFD);
        ServerFD = -1;
    }
}
