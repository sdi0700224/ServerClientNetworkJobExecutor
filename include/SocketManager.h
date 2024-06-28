#pragma once
#include <string>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
using namespace std;

class SocketManager
{
private:
    int ServerFD;
    int ClientFD;
    struct addrinfo* AddrInfo;
    
    void FreeResources();
    uint64_t Htonll(uint64_t value);
    uint64_t Ntohll(uint64_t value);

public:
    SocketManager();
    ~SocketManager();

    bool ResolveAndConnect(const string& hostname, const string& port);
    bool SetupServer(const string& port);
    int AcceptConnection();
    bool SendMessage(int socketFD, const string& message);
    bool ReceiveMessage(int socketFD, string& message);
    bool ReceiveFileData(int socketFD);
    bool SendFileData(int socketFD, const string& fileName);
    int GetClientSocketFD() const;
    void CloseServerSocket();
};
