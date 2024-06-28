#include "Server.h"
#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <portnum> <bufferSize> <threadPoolSize>" << endl;
        return EXIT_FAILURE;
    }

    int port = stoi(argv[1]);
    int bufferSize = stoi(argv[2]);
    int threadPoolSize = stoi(argv[3]);

    if (bufferSize <= 0)
    {
        cerr << "Error: bufferSize must be a positive integer." << endl;
        return EXIT_FAILURE;
    }
    if (threadPoolSize <= 0)
    {
        cerr << "Error: threadPoolSize must be a positive integer." << endl;
        return EXIT_FAILURE;
    }

    Server server(port, bufferSize, threadPoolSize);
    server.Start();

    return EXIT_SUCCESS;
}
