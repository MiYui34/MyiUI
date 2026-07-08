#include "bridge/overlay_command_queue.h"

#include "bridge/native_query.h"
#include "jvm/jvm_log.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace myiui::bridge {
namespace {

std::mutex g_mutex;
std::condition_variable g_cv;
std::queue<std::string> g_queue;
std::atomic<bool> g_running{false};
std::thread g_worker;

void WorkerLoop() {
    for (;;) {
        std::string command;
        {
            std::unique_lock lock(g_mutex);
            g_cv.wait(lock, [] { return !g_running.load(std::memory_order_acquire) || !g_queue.empty(); });
            if (!g_running.load(std::memory_order_acquire) && g_queue.empty()) {
                return;
            }
            command = std::move(g_queue.front());
            g_queue.pop();
        }
        if (!ActionJava(command)) {
            jvm::SpikeLog(L"[command_queue] ActionJava failed");
        }
    }
}

void EnsureWorker() {
    bool expected = false;
    if (g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        g_worker = std::thread(WorkerLoop);
    }
}

}  // namespace

void EnqueueOverlayCommand(std::string command) {
    EnsureWorker();
    {
        std::lock_guard lock(g_mutex);
        g_queue.push(std::move(command));
    }
    g_cv.notify_one();
}

void ShutdownOverlayCommandQueue() {
    if (!g_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    g_cv.notify_all();
    if (g_worker.joinable()) {
        g_worker.join();
    }
}

}  // namespace myiui::bridge
