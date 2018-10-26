#include "Node.h"

namespace uS {

// this should be Node
void NodeData::asyncCallback(Async *async)
{
    NodeData *nodeData = (NodeData *) async->getData();

    nodeData->asyncMutex->lock();
    for (Poll *p : nodeData->transferQueue) {
        Socket *s = (Socket *) p;
        TransferData *transferData = (TransferData *) s->getUserData();

        s->reInit(nodeData->loop, transferData->fd);
        s->setCb(transferData->pollCb);
        s->start(nodeData->loop, s, s->setPoll(transferData->pollEvents));

        s->nodeData = transferData->destination;
        s->setUserData(transferData->userData);
        auto *transferCb = transferData->transferCb;

        delete transferData;
        transferCb(s);
    }

    for (Poll *p : nodeData->changePollQueue) {
        Socket *s = (Socket *) p;
        s->change(s->nodeData->loop, s, s->getPoll());
    }

    nodeData->changePollQueue.clear();
    nodeData->transferQueue.clear();
    nodeData->asyncMutex->unlock();
}

Node::Node(int recvLength, int prePadding, int postPadding, bool useDefaultLoop) {
    nodeData = new NodeData;
    nodeData->recvBufferMemoryBlock = new unsigned char[recvLength];
    nodeData->recvBuffer = nodeData->recvBufferMemoryBlock + prePadding;
    nodeData->recvLength = recvLength - prePadding - postPadding;

    nodeData->tid = pthread_self();
    loop = Loop::createLoop(useDefaultLoop);

    // each node has a context
    nodeData->netContext = new Context();

    nodeData->loop = loop;
    nodeData->asyncMutex = &asyncMutex;

    int indices = NodeData::getMemoryBlockIndex(NodeData::preAllocMaxSize) + 1;
    nodeData->preAlloc = new char*[indices];
    for (int i = 0; i < indices; i++) {
        nodeData->preAlloc[i] = nullptr;
    }

    nodeData->clientContext = std::shared_ptr<mbedtls_ssl_context>(new mbedtls_ssl_context, mbedtls_ssl_free);
}

void Node::run() {
    nodeData->tid = pthread_self();
    loop->run();
}

void Node::poll() {
    loop->poll();
}

Node::~Node() {
    delete [] nodeData->recvBufferMemoryBlock;

    int indices = NodeData::getMemoryBlockIndex(NodeData::preAllocMaxSize) + 1;
    for (int i = 0; i < indices; i++) {
        if (nodeData->preAlloc[i]) {
            delete [] nodeData->preAlloc[i];
        }
    }
    delete [] nodeData->preAlloc;
    delete nodeData->netContext;
    delete nodeData;
    loop->destroy();
}

}
