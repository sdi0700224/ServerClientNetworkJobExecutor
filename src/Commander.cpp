#include "Commander.h"
#include <iostream>

Commander::Commander(const string& serverName, const string& port)
    : ServerName(serverName), Port(port)
{
    if (!SocketController.ResolveAndConnect(ServerName, Port))
    {
        cerr << "Failed to connect to server " << ServerName << " on port " << Port << endl;
        exit(EXIT_FAILURE);
    }
}

Commander::~Commander()
{
}

void Commander::IssueJob(const string& job)
{
    auto command = "issueJob " + job;
    SendCommand(command);

    ReceiveResponse();
    string response = ReceiveResponse();
    if (response.find("output start") != string::npos)
    {
        int clientFD = SocketController.GetClientSocketFD();
        SocketController.ReceiveFileData(clientFD);
        ReceiveResponse();
    }
}

void Commander::SetConcurrency(int level)
{
    auto command = "setConcurrency " + to_string(level);
    SendCommand(command);
    ReceiveResponse();
}

void Commander::StopJob(const string& jobId)
{
    auto command = "stop " + jobId;
    SendCommand(command);
    ReceiveResponse();
}

void Commander::PollJobs()
{
    SendCommand("poll");
    ReceiveResponse();
}

void Commander::ExitServer()
{
    SendCommand("exit");
    ReceiveResponse();
}

void Commander::SendCommand(const string& command)
{
    int clientFD = SocketController.GetClientSocketFD();
    if (!SocketController.SendMessage(clientFD, command))
    {
        cerr << "Failed to send command: " << command << endl;
    }
}

string Commander::ReceiveResponse()
{
    int clientFD = SocketController.GetClientSocketFD();
    string response;
    if (!SocketController.ReceiveMessage(clientFD, response))
    {
        cerr << "Failed to receive response" << endl;
    }

    cout << response << endl;
    return response;
}
