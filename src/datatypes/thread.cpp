#include "datatypes/thread.h"

using namespace std;


DATATYPES_NAMESPACE_BEGIN

std::function<void(Thread*)> Thread::onStartingRunningThread = [] (Thread* pt) {
    fprintf(stderr, "Warning: Attempt to start a running thread Thread(0x%016x)\n", 
            (uint64_t) pt);
};

std::function<void(Thread*)> Thread::onStoppingStoppedThread = [] (Thread* pt) {
    fprintf(stderr, "Warning: Attempt to stop a stopped thread Thread(0x%016x)\n", 
            (uint64_t) pt);
};

std::function<void(Thread*)> Thread::onTerminatingStoppedThread = [] (Thread* pt) {
    fprintf(stderr, "Warning: Attempt to terminate a stopped thread Thread(0x%016x)\n", 
            (uint64_t) pt);
};

std::function<void(Thread*)> Thread::onStartingNullTask = [] (Thread* pt) {
    fprintf(stderr, "Warning: Attempt to start a thread with null task Thread(0x%016x)\n", (uint64_t) pt);
};

std::function<void(Thread*)> Thread::onForceTerminate = [] (Thread* pt) {
    fprintf(stderr, "Warning: Force terminating thread Thread(0x%016x)\n", 
            (uint64_t) pt);
};

std::function<void(Thread*)> Thread::onGetIdFailed = [] (Thread* pt) {
    fprintf(stderr, "Warning: Thread(0x%016x) getId() failed\n", (uint64_t) pt);
};