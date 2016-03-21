#include <iostream>
using namespace std;

#include "Server.h"

Server server(3000);

void Server::onFragment(void *p, char *fragment, size_t length)
{
    server.send(p, fragment, length);
}

int main()
{
    server.run();
    return 0;
}
