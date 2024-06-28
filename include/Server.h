#pragma once
#include "SocketManager.h"
#include <vector>
#include <queue>
#include <pthread.h>
using namespace std;

class Server
{
private:
    int Port;
    int BufferSize;
    int ThreadPoolSize;
    int ConcurrencyLevel;
    bool IsRunning;
    int JobCounter;
    int ActiveWorkers;
    vector<pthread_t> WorkerThreads;
    pthread_mutex_t QueueMutex;
    pthread_cond_t JobAvailable;
    pthread_cond_t SpaceAvailable;
    queue<tuple<string, string, int>> JobQueue;
    SocketManager SocketController;

    static void* WorkerThreadFunction(void* arg);
    static void* HandleClient(void* arg);
    void ProcessJob(int clientSocket, const string& job, const string& jobID);
    void HandleRemainingJobs();
    void SetConcurrency(int newLevel);
    void StopServer();
    void RemoveJob(const string& jobID, int clientSocket);

public:
    Server(int port, int bufferSize, int threadPoolSize);
    ~Server();
    
    void Start();
};

struct ClientHandlerArgs
{
    Server* ServerInstance;
    int ClientSocket;
};


