#pragma once
#include "SocketManager.h"
#include <string>
#include <vector>
using namespace std;

class Commander {
private:    
    SocketManager SocketController;
    string ServerName;
    string Port;

    string ReceiveResponse();
    void SendCommand(const string& command);

public:
    Commander(const string& serverName, const string& port);
    ~Commander();

    void IssueJob(const string& job);
    void SetConcurrency(int level);
    void StopJob(const string& jobId);
    void PollJobs();
    void ExitServer();
};
