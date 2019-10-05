#include "App.h"
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <queue>
#include <string>

int running = 1;
std::mutex qmutex;
std::queue<uWS::AsyncProcData*> q;

bool finish_request(uWS::AsyncProcData* pd){
    // calling this function in the separate thread
    if (pd->procDataMutex.try_lock()){
        pd->asyncProcStarted = true;
        pd->out = pd->asyncInput;
        pd->procDataMutex.unlock();
        return true;
    }
    std::cout << "failed to lock procdatamutex, could not modify procdata" << std::endl;
    return false;
}

void thread_2_batch_function(){
    // calling this function in the separate thread
    while(running){
        usleep(100);
        qmutex.lock();
        for (int i=0;!q.empty();i++){
            //std::cout << "thread 2 handling queue now" << std::endl;
            auto pd = q.front();
            finish_request(pd);
            q.pop();
        }
        qmutex.unlock();
    }
}


template <bool SSL>
void asyncProcCheck(uWS::HttpResponse<SSL>* res){
    if (res->getProcData()->asyncProcStarted && res->getProcData()->procDataMutex.try_lock()){
        res->getProcData()->procDataMutex.unlock();
        uWS::AsyncProcData* pd = res->getProcData();
        res->end(res->getProcData()->out);
        delete pd;
    }
    else{
        if (res->getProcData() != nullptr){
            uWS::Loop* loop = uWS::Loop::get();
            loop->defer([res](){
                asyncProcCheck(res);
            });
        }
    }
}

template <bool SSL>
void load_queue(uWS::HttpRequest* req, uWS::HttpResponse<SSL>* res, uint count){
    uWS::AsyncProcData* pd = new uWS::AsyncProcData;

    res->setProcData(pd);
    //res->getProcData() = pd;
    pd->asyncProcStarted = false;
    pd->asyncInput = "This is response number: " + std::to_string(count) + "\n";
    qmutex.lock();
    q.push(pd);
    qmutex.unlock();
    res->onWritable([res](int offset) {
        asyncProcCheck(res);
        // why do we return false?  don't know, it was in AsyncFileStream.h
        return false;
    })->onAborted([]() {
        std::cout << "ABORTED!" << std::endl;
    });
    //asyncProcCheck(res);
    uWS::Loop* loop = uWS::Loop::get();
    loop->defer([res](){
        asyncProcCheck(res);
    });
}

template <bool SSL>
void print_q_size(uWS::HttpResponse<SSL>* res){
    res->end(std::to_string(q.size()));
}

int main() {
    std::cout << "thread id for main function is: " << std::this_thread::get_id();

    std::thread t2(thread_2_batch_function);
    /* Overly simple batch processing app, using multiple threads for input into a queue
        with a single queue output.  This is to have a simple illustration of the async
        capabilities of uWebsockets.  It is not as performant as it could be.
    */
    std::vector<std::thread *> threads(std::thread::hardware_concurrency());

    std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread *t) {
        return new std::thread([]() {
            std::atomic<uint> count = 0;
            uWS::App().get("/hello", [&count](auto *res, auto *req) {
                load_queue(req, res, count++);
            }).get("/check", [&count](auto *res, auto *req) {
                print_q_size(res);
            }).get("/stopbatch", [&count](auto *res, auto *req) {
                res->end("exiting thread 2");
                running=0;
            }).listen(3000, [](auto *token) {
                if (token) {
                    std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << 3000 << std::endl;
                } else {
                    std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
                }
            }).run();

        });
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
        t->join();
    });
    t2.join();
}
