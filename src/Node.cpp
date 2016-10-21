#include "Node.h"

namespace uS {

void NodeData::asyncCallback(uv_async_t *async)
{
    NodeData *nodeData = (NodeData *) async->data;

    nodeData->asyncMutex->lock();
    for (TransferData transferData : nodeData->transferQueue) {
        uv_poll_init_socket(nodeData->loop, transferData.p, transferData.fd);
        transferData.p->data = transferData.socketData;
        transferData.socketData->nodeData = nodeData;
        uv_poll_start(transferData.p, transferData.socketData->poll, transferData.pollCb);

        transferData.cb(transferData.p);
    }

    for (uv_poll_t *p : nodeData->changePollQueue) {
        SocketData *socketData = (SocketData *) p->data;
        uv_poll_start(p, socketData->poll, /*p->poll_cb*/ Socket(p).getPollCallback());
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

void Node::run() {
    nodeData->tid = pthread_self();

    uv_run(loop, UV_RUN_DEFAULT);
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
