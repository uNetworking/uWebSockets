#include <iostream>
using namespace std;

#include "Server.h"

Server server(3000);

void Server::onFragment(void *p, char *fragment, size_t length)
{
    //cout << "Fragment: " << string(fragment, length) << endl;
    server.send(p, fragment, length);
}

int main()
{
    server.run();
    return 0;
}
