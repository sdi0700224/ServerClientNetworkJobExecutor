#include "Server.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <tuple>
#include <string>
using namespace std;

Server::Server(int port, int bufferSize, int threadPoolSize)
    : Port(port), BufferSize(bufferSize), ThreadPoolSize(threadPoolSize), 
      ConcurrencyLevel(1), IsRunning(true), JobCounter(0), ActiveWorkers(0)
{
    pthread_mutex_init(&QueueMutex, nullptr);
    pthread_cond_init(&JobAvailable, nullptr);
    pthread_cond_init(&SpaceAvailable, nullptr);

    WorkerThreads.reserve(threadPoolSize);
    for (int i = 0; i < threadPoolSize; i++)
    {
        pthread_t workerThread;
        pthread_create(&workerThread, nullptr, &Server::WorkerThreadFunction, this);
        WorkerThreads.push_back(workerThread);
    }
}

Server::~Server()
{
    StopServer();
    for (auto& thread : WorkerThreads)
    {
        pthread_join(thread, nullptr);
    }

    pthread_mutex_destroy(&QueueMutex);
    pthread_cond_destroy(&JobAvailable);
    pthread_cond_destroy(&SpaceAvailable);
}

void Server::Start()
{
    if (!SocketController.SetupServer(to_string(Port)))
    {
        cerr << "Failed to setup server on port " << Port << endl;
        return;
    }

    while (IsRunning)
    {
        int clientSocket = SocketController.AcceptConnection();
        if (clientSocket >= 0)
        {
            ClientHandlerArgs* args = new ClientHandlerArgs{ this, clientSocket };
            pthread_t clientThread;
            pthread_create(&clientThread, nullptr, &Server::HandleClient, args);
            pthread_detach(clientThread);
        }
    }
}

void* Server::HandleClient(void* arg)
{
    ClientHandlerArgs* args = static_cast<ClientHandlerArgs*>(arg);
    Server* serverInstance = args->ServerInstance;
    int clientSocket = args->ClientSocket;
    delete args;

    string command;
    if (serverInstance->SocketController.ReceiveMessage(clientSocket, command))
    {
        if (command.find("issueJob") == 0)
        {
            string job = command.substr(9);
            string jobID = "job_" + to_string(serverInstance->JobCounter++);
            {
                pthread_mutex_lock(&serverInstance->QueueMutex);
                while ((int)serverInstance->JobQueue.size() >= serverInstance->BufferSize)
                {
                    pthread_cond_wait(&serverInstance->SpaceAvailable, &serverInstance->QueueMutex);
                }
                if (!serverInstance->IsRunning)
                {
                    pthread_mutex_unlock(&serverInstance->QueueMutex);
                    close(clientSocket);
                    return nullptr;
                }
                serverInstance->JobQueue.emplace(jobID, job, clientSocket);
                string response = "JOB " + jobID + ", " + job + " SUBMITTED\n";
                if (clientSocket >= 0)
                {
                    serverInstance->SocketController.SendMessage(clientSocket, response);
                }
                pthread_cond_signal(&serverInstance->JobAvailable);
                pthread_mutex_unlock(&serverInstance->QueueMutex);
            }
        }
        else if (command.find("setConcurrency") == 0)
        {
            int newLevel = stoi(command.substr(14));
            serverInstance->SetConcurrency(newLevel);
            string response = "CONCURRENCY SET AT " + to_string(newLevel) + "\n";
            if (clientSocket >= 0)
            {
                serverInstance->SocketController.SendMessage(clientSocket, response);
            }
            close(clientSocket);
        }
        else if (command.find("stop") == 0)
        {
            string jobID = command.substr(5);
            serverInstance->RemoveJob(jobID, clientSocket);
            close(clientSocket);
        }
        else if (command.find("poll") == 0)
        {
            pthread_mutex_lock(&serverInstance->QueueMutex);
            queue<tuple<string, string, int>> tempQueue = serverInstance->JobQueue;
            pthread_mutex_unlock(&serverInstance->QueueMutex);
            
            string response;
            while (!tempQueue.empty())
            {
                auto job = tempQueue.front();
                response += get<0>(job) + ", " + get<1>(job) + "\n";
                tempQueue.pop();
            }
            if (clientSocket >= 0)
            {
                serverInstance->SocketController.SendMessage(clientSocket, response);
            }
            close(clientSocket);
        }
        else if (command.find("exit") == 0)
        {
            string response = "SERVER TERMINATED\n";
            if (clientSocket >= 0)
            {
                serverInstance->SocketController.SendMessage(clientSocket, response);
            }
            serverInstance->StopServer();
            close(clientSocket);
        }
    }
    else
    {
        cerr << "Failed to receive command from client" << endl;
        close(clientSocket);
    }

    return nullptr;
}

void* Server::WorkerThreadFunction(void* arg)
{
    Server* serverInstance = static_cast<Server*>(arg);

    while (serverInstance->IsRunning)
    {
        tuple<string, string, int> job;
        {
            pthread_mutex_lock(&serverInstance->QueueMutex);
            while ((serverInstance->JobQueue.empty() || serverInstance->ActiveWorkers >= serverInstance->ConcurrencyLevel) && serverInstance->IsRunning)
            {
                pthread_cond_wait(&serverInstance->JobAvailable, &serverInstance->QueueMutex);
            }
            if (!serverInstance->IsRunning)
            {
                pthread_mutex_unlock(&serverInstance->QueueMutex);
                return nullptr;
            }

            job = serverInstance->JobQueue.front();
            serverInstance->JobQueue.pop();
            serverInstance->ActiveWorkers++;
            pthread_cond_signal(&serverInstance->SpaceAvailable);
            pthread_mutex_unlock(&serverInstance->QueueMutex);
        }

        serverInstance->ProcessJob(get<2>(job), get<1>(job), get<0>(job));

        pthread_mutex_lock(&serverInstance->QueueMutex);
        serverInstance->ActiveWorkers--;
        pthread_cond_signal(&serverInstance->JobAvailable);
        pthread_mutex_unlock(&serverInstance->QueueMutex);
    }
    return nullptr;
}

void Server::ProcessJob(int clientSocket, const string& job, const string& jobID)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        string outputFile = to_string(getpid()) + ".output";
        int fd = open(outputFile.c_str(), O_WRONLY | O_CREAT, 0666);
        if (fd == -1)
        {
            perror("Failed to open output file");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            perror("Failed to duplicate file descriptor to STDOUT");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDERR_FILENO) == -1)
        {
            perror("Failed to duplicate file descriptor to STDERR");
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
        execlp("/bin/sh", "sh", "-c", job.c_str(), nullptr);
        perror("Failed to execute command");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish

        string outputFile = to_string(pid) + ".output";
        if (clientSocket >= 0)
        {
            string responseHeader = "-----" + jobID + " output start------\n";
            SocketController.SendMessage(clientSocket, responseHeader);
            if (!SocketController.SendFileData(clientSocket, outputFile))
            {
                cerr << "Failed to send file data." << endl;
            }
            string responseFooter = "-----" + jobID + " output end------\n";
            SocketController.SendMessage(clientSocket, responseFooter);
            
            close(clientSocket);
        }
        remove(outputFile.c_str());
    }
    else
    {
        cerr << "Error: fork() failed to create a new process for job: " << job << endl;
        string response = "Error: Unable to execute job: " + job + "\n";
        if (clientSocket >= 0)
        {
            SocketController.SendMessage(clientSocket, response);
        }
        close(clientSocket);
    }
}

void Server::HandleRemainingJobs()
{
    while (!JobQueue.empty())
    {
        auto job = JobQueue.front();
        JobQueue.pop();

        string response = "SERVER TERMINATED BEFORE EXECUTION";
        if (get<2>(job) >= 0)
        {
            SocketController.SendMessage(get<2>(job), response);
            close(get<2>(job));
        }
    }
}

void Server::SetConcurrency(int newLevel)
{
    pthread_mutex_lock(&QueueMutex);
    ConcurrencyLevel = newLevel;
    pthread_cond_broadcast(&JobAvailable);
    pthread_mutex_unlock(&QueueMutex);
}

void Server::StopServer()
{
    pthread_mutex_lock(&QueueMutex);
    if (IsRunning)
    {
        IsRunning = false;
        pthread_cond_broadcast(&JobAvailable);
        HandleRemainingJobs();
        pthread_cond_broadcast(&SpaceAvailable);
        SocketController.CloseServerSocket();
        cout << "SERVER TERMINATED" << endl;
    }
    pthread_mutex_unlock(&QueueMutex);
}

void Server::RemoveJob(const string& jobID, int clientSocket)
{
    pthread_mutex_lock(&QueueMutex);
    queue<tuple<string, string, int>> tempQueue;
    bool found = false;
    
    while (!JobQueue.empty())
    {
        auto job = JobQueue.front();
        JobQueue.pop();
        
        if (get<0>(job) == jobID)
        {
            string response = "JOB " + jobID + " REMOVED\n";
            if (clientSocket >= 0)
            {
                SocketController.SendMessage(clientSocket, response);
            }
            close(clientSocket);
            if (get<2>(job) >= 0)
            {
                SocketController.SendMessage(get<2>(job), response);
            }
            close(get<2>(job));
            found = true;
            pthread_cond_signal(&SpaceAvailable);
            break;
        }
        else
        {
            tempQueue.push(job);
        }
    }
    
    while (!tempQueue.empty()) // Put the remaining jobs back in the original queue
    {
        JobQueue.push(tempQueue.front());
        tempQueue.pop();
    }
    
    if (!found)
    {
        string response = "JOB " + jobID + " NOT FOUND\n";
        if (clientSocket >= 0)
        {
            SocketController.SendMessage(clientSocket, response);
        }
    }
    
    pthread_mutex_unlock(&QueueMutex);
}
