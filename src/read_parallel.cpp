#include "read_parallel.h"
#include "ply_bbox_reader.h"

#include <atomic>
#include <thread>
#include <exception>
#include <mutex>

std::vector<BBox> readAllBBoxesParallel(const std::vector<std::string>& files, int numThreads) {
    const int n = static_cast<int>(files.size());
    std::vector<BBox> out(n);

    std::atomic<int> next{0};

    std::exception_ptr firstErr = nullptr;
    std::mutex errMtx;

    auto worker = [&]() {
        while (true) {
            int i = next.fetch_add(1);
            if (i >= n) break;

            try {
                out[i] = readPlyBBox(files[i], i);
            } catch (...) {
                std::lock_guard<std::mutex> lk(errMtx);
                if (!firstErr) firstErr = std::current_exception();
                return; // corta este worker
            }
        }
    };

    const int t = std::min(numThreads, n);
    std::vector<std::thread> threads;
    threads.reserve(t);

    for (int k = 0; k < t; ++k) threads.emplace_back(worker);
    for (auto& th : threads) th.join();

    if (firstErr) std::rethrow_exception(firstErr);

    return out;
}
