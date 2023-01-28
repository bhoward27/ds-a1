#include "core/graph.h"
#include "core/utils.h"
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>

using std::thread;
using std::mutex;
using std::vector;
using std::ref;
using std::atomic;

#ifdef USE_INT
#define INIT_PAGE_RANK 100000
#define EPSILON 1000
#define PAGE_RANK(x) (15000 + (5 * x) / 6)
#define CHANGE_IN_PAGE_RANK(x, y) std::abs(x - y)
typedef int64_t PageRankType;
#else
#define INIT_PAGE_RANK 1.0
#define EPSILON 0.01
#define DAMPING 0.85
#define PAGE_RANK(x) (1 - DAMPING + DAMPING * x)
#define CHANGE_IN_PAGE_RANK(x, y) std::fabs(x - y)
typedef float PageRankType;
#endif

void thread_func(int tid,
                vector<double>& thread_times,
                Graph& g,
                const uintV first_vertex,
                const uintV final_vertex,
                int max_iters,
                PageRankType* pr_curr,
                atomic<PageRankType>* pr_next,
                CustomBarrier& barrier,
                mutex& lock)
{
    timer t;
    t.start();
    for (int iter = 0; iter < max_iters; iter++) {
        for (uintV u = first_vertex; u <= final_vertex; u++) {
            uintE out_degree = g.vertices_[u].getOutDegree();
            for (uintE i = 0; i < out_degree; i++) {
                uintV v = g.vertices_[u].getOutNeighbor(i);
                PageRankType expected = pr_next[v];
                PageRankType desired;
                do {
                    desired = expected + pr_curr[u] / out_degree;
                } while(!pr_next[v].compare_exchange_weak(expected, desired));
            }
        }
        barrier.wait();
        for (uintV v = first_vertex; v <= final_vertex; v++) {
            // No lock needed here, since v is only from this thread's subset of vertices.
            pr_next[v] = PAGE_RANK(pr_next[v]);
            pr_curr[v] = pr_next[v];
            pr_next[v] = 0.0;
        }
        barrier.wait();
    }
    thread_times[tid] = t.stop();
}

void pageRankParallel(Graph &g, int max_iters, int num_threads) {
    uintV n = g.n_;

    PageRankType* pr_curr = new PageRankType[n];
    atomic<PageRankType>* pr_next = new atomic<PageRankType>[n];

    for (uintV i = 0; i < n; i++) {
        pr_curr[i] = INIT_PAGE_RANK;
        pr_next[i] = 0.0;
    }

    // Push based pagerank
    timer t1;
    double time_taken = 0.0;
    // Create threads and distribute the work across T threads
    // -------------------------------------------------------------------
    vector<thread> threads;
    vector<double> thread_times;
    CustomBarrier barrier(num_threads);
    mutex lock;

    t1.start();
    uintV quotient = n / num_threads;
    uintV remainder = n % num_threads;
    uintV step = quotient;
    uintV final_step = (remainder) ? step + remainder : step;
    uintV first_vertex = 0;
    uintV final_vertex = step - 1;
    for (int tid = 0; tid < num_threads; tid++) {
        thread_times.push_back(0.0);
        threads.push_back(
            thread(
                thread_func,
                tid,
                ref(thread_times),
                ref(g),
                first_vertex,
                final_vertex,
                max_iters,
                pr_curr,
                pr_next,
                ref(barrier),
                ref(lock)
            )
        );

        first_vertex = final_vertex + 1;
        if (tid == num_threads - 2) {
            final_vertex += final_step;
        }
        else {
            final_vertex += step;
        }
    }
    for (auto& thread : threads) {
        thread.join();
    }
    time_taken = t1.stop();

    std::cout << "thread_id, time_taken\n";
    for (int tid = 0; tid < num_threads; tid++) {
        std::cout << tid << ", " << thread_times[tid] << "\n";
    }

    PageRankType sum_of_page_ranks = 0;
    for (uintV u = 0; u < n; u++) {
        sum_of_page_ranks += pr_curr[u];
    }
    std::cout << "Sum of page rank : " << sum_of_page_ranks << "\n";
    std::cout << "Time taken (in seconds) : " << time_taken << "\n";
    delete[] pr_curr;
    delete[] pr_next;
}


int main(int argc, char *argv[]) {
    cxxopts::Options options(
            "page_rank_push",
            "Calculate page_rank using serial and parallel execution");
    options.add_options(
            "",
            {
                    {"nWorkers", "Number of workers",
                     cxxopts::value<uint>()->default_value(DEFAULT_NUMBER_OF_WORKERS)},
                    {"nIterations", "Maximum number of iterations",
                     cxxopts::value<uint>()->default_value(DEFAULT_MAX_ITER)},
                    {"inputFile", "Input graph file path",
                     cxxopts::value<std::string>()->default_value(
                    "/scratch/input_graphs/roadNet-CA")},
            });

    auto cl_options = options.parse(argc, argv);
    uint n_workers = cl_options["nWorkers"].as<uint>();
    uint max_iterations = cl_options["nIterations"].as<uint>();
    std::string input_file_path = cl_options["inputFile"].as<std::string>();

#ifdef USE_INT
    std::cout << "Using INT\n";
#else
    std::cout << "Using FLOAT\n";
#endif
    std::cout << std::fixed;
    std::cout << "Number of workers : " << n_workers << "\n";

    Graph g;
    std::cout << "Reading graph\n";
    g.readGraphFromBinary<int>(input_file_path);
    std::cout << "Created graph\n";
    pageRankParallel(g, max_iterations, n_workers);

    return 0;
}