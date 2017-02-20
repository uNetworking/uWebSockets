#include "Node.h"

namespace uS {

void NodeData::asyncCallback(uv_async_t *async)
{
    NodeData *nodeData = (NodeData *) async->data;

    nodeData->asyncMutex->lock();
    for (TransferData transferData : nodeData->transferQueue) {
        transferData.p->init(nodeData->loop, transferData.fd);
        transferData.p->setCb(transferData.pollCb);
        transferData.p->start(transferData.socketData->poll);
        transferData.p->setData(transferData.socketData);
        transferData.socketData->nodeData = nodeData;
        transferData.cb(transferData.p);
    }

    for (Poll *p : nodeData->changePollQueue) {
        SocketData *socketData = (SocketData *) p->getData();
        p->change(socketData->poll);
    }

    nodeData->changePollQueue.clear();
    nodeData->transferQueue.clear();
    nodeData->asyncMutex->unlock();
}

Node::Node(int recvLength, int prePadding, int postPadding, bool useDefaultLoop) {
    nodeData = new NodeData;
    nodeData->recvBufferMemoryBlock = new char[recvLength];
    nodeData->recvBuffer = nodeData->recvBufferMemoryBlock + prePadding;
    nodeData->recvLength = recvLength - prePadding - postPadding;

    nodeData->tid = pthread_self();

    if (useDefaultLoop) {
        loop = uv_default_loop();
    } else {
        loop = uv_loop_new();
    }

    nodeData->loop = loop;
    nodeData->asyncMutex = &asyncMutex;

    int indices = NodeData::getMemoryBlockIndex(NodeData::preAllocMaxSize) + 1;
    nodeData->preAlloc = new char*[indices];
    for (int i = 0; i < indices; i++) {
        nodeData->preAlloc[i] = nullptr;
    }

    nodeData->clientContext = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_options(nodeData->clientContext, SSL_OP_NO_SSLv3);
}

int Node::run(uv_run_mode mode) {
    nodeData->tid = pthread_self();

    return uv_run(loop, mode);
}

Node::~Node() {
    delete [] nodeData->recvBufferMemoryBlock;
    SSL_CTX_free(nodeData->clientContext);

    int indices = NodeData::getMemoryBlockIndex(NodeData::preAllocMaxSize) + 1;
    for (int i = 0; i < indices; i++) {
        if (nodeData->preAlloc[i]) {
            delete [] nodeData->preAlloc[i];
        }
    }
    delete [] nodeData->preAlloc;

    delete nodeData;

    if (loop != uv_default_loop()) {
        uv_loop_delete(loop);
    }
}

}
