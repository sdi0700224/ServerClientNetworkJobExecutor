#include "Commander.h"
#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0] << " <serverName> <portNum> <jobCommanderInputCommand> [arguments]" << endl;
        return EXIT_FAILURE;
    }

    string serverName = argv[1];
    string port = argv[2];
    Commander commander(serverName, port);
    string command = argv[3];

    if (command == "issueJob" && argc >= 5)
    {
        string job;
        for (int i = 4; i < argc; i++)
        {
            job += string(argv[i]) + " ";
        }
        commander.IssueJob(job);
    }
    else if (command == "setConcurrency" && argc == 5)
    {
        auto level = stoi(argv[4]);
        commander.SetConcurrency(level);
    }
    else if (command == "stop" && argc == 5)
    {
        string jobId = argv[4];
        commander.StopJob(jobId);
    }
    else if (command == "poll" && argc == 4)
    {
        commander.PollJobs();
    }
    else if (command == "exit" && argc == 4)
    {
        commander.ExitServer();
    }
    else
    {
        cerr << "Invalid command or wrong number of arguments." << endl;
        cerr << "Usage examples:" << endl;
        cerr << argv[0] << " issueJob <command>" << endl;
        cerr << argv[0] << " setConcurrency <level>" << endl;
        cerr << argv[0] << " stop <jobId>" << endl;
        cerr << argv[0] << " poll [running|queued]" << endl;
        cerr << argv[0] << " exit" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
