#include "core/graph.h"
#include "core/utils.h"
#include <future>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>

using std::vector;
using std::thread;
using std::atomic;
using std::ref;

typedef struct {
    long non_unique_triangles;
    double time_taken;
} Result;

long countTriangles(uintV *array1, uintE len1, uintV *array2, uintE len2, uintV u, uintV v)
{
    uintE i = 0, j = 0; // indexes for array1 and array2
    long count = 0;

    if (u == v)
        return count;

    while ((i < len1) && (j < len2)) {
        if (array1[i] == array2[j]) {
            if ((array1[i] != u) && (array1[i] != v)) {
                count++;
            }
            i++;
            j++;
        } else if (array1[i] < array2[j]) {
            i++;
        } else {
            j++;
        }
    }
    return count;
}

void thread_func(Graph& g,
                 uintV first_vertex,
                 uintV final_vertex,
                 uint tid,
                 vector<Result>& thread_results,
                 atomic<long>& global_triangle_count)
{
        // Process each edge <u,v>
    timer t;
    t.start();
    long local_triangle_count = 0;
    for (uintV u = first_vertex; u <= final_vertex; u++) {
        // For each outNeighbor v, find the intersection of inNeighbor(u) and
        // outNeighbor(v)
        uintE out_degree = g.vertices_[u].getOutDegree();
        for (uintE i = 0; i < out_degree; i++) {
            uintV v = g.vertices_[u].getOutNeighbor(i);
            local_triangle_count += countTriangles(g.vertices_[u].getInNeighbors(),
                                             g.vertices_[u].getInDegree(),
                                             g.vertices_[v].getOutNeighbors(),
                                             g.vertices_[v].getOutDegree(),
                                             u,
                                             v);
        }
    }

    // Atomically add to the global triangle count.
    long expected = global_triangle_count;
    long desired;
    do {
        desired = expected + local_triangle_count;
    } while(!global_triangle_count.compare_exchange_weak(expected, desired));

    Result res = {local_triangle_count, t.stop()};
    thread_results[tid] = res;
}

void triangleCountParallel(Graph &g, uint num_threads)
{
    uintV n = g.n_;
    atomic<long> triangle_count(0);
    double time_taken = 0.0;
    timer t1;
    vector<Result> thread_results;
    vector<thread> threads;

    uintV quotient = n / num_threads;
    uintV remainder = n % num_threads;
    uintV step = quotient;
    uintV final_step = (remainder) ? step + remainder : step;
    uintV first_vertex = 0;
    uintV final_vertex = step - 1;

    // The outNghs and inNghs for a given vertex are already sorted

    // Create threads and distribute the work across T threads
    // -------------------------------------------------------------------
    t1.start();
    for (uint tid = 0; tid < num_threads; tid++) {
        Result init_result = {0};
        thread_results.push_back(init_result);
        threads.push_back(
            thread(
                thread_func,
                ref(g),
                first_vertex,
                final_vertex,
                tid,
                ref(thread_results),
                ref(triangle_count)
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
    for (auto& lwp : threads) {
        lwp.join();
    }
    time_taken = t1.stop();
    // -------------------------------------------------------------------
    // Here, you can just print the number of non-unique triangles counted by each
    // thread
    std::cout << "thread_id, triangle_count, time_taken\n";
    for (uint tid = 0; tid < num_threads; tid++) {
        Result res = thread_results[tid];
        std::cout << tid << ", " << res.non_unique_triangles << ", " << res.time_taken << "\n";
    }
    // Print the
    // above statistics for each thread
    // Example output for 2 threads:
    // thread_id, triangle_count, time_taken
    // 0, 100, 0.12
    // 1, 102, 0.12

    // Print the overall statistics
    std::cout << "Number of triangles : " << triangle_count << "\n";
    std::cout << "Number of unique triangles : " << triangle_count / 3 << "\n";
    std::cout << "Time taken (in seconds) : " << std::setprecision(TIME_PRECISION)
                        << time_taken << "\n";
}

int main(int argc, char *argv[])
{
    cxxopts::Options options(
            "triangle_counting_serial",
            "Count the number of triangles using serial and parallel execution");
    options.add_options(
            "custom",
            {
                    {"nWorkers", "Number of workers",
                     cxxopts::value<uint>()->default_value(DEFAULT_NUMBER_OF_WORKERS)},
                    {"inputFile", "Input graph file path",
                     cxxopts::value<std::string>()->default_value(
                    // TODO: Add back in!         "/scratch/input_graphs/roadNet-CA")},
                    "/home/ben/Documents/SFU/cmpt-431/assignments/1/default_page_rank_graph/roadNet-CA")},
            });

    auto cl_options = options.parse(argc, argv);
    uint n_workers = cl_options["nWorkers"].as<uint>();
    std::string input_file_path = cl_options["inputFile"].as<std::string>();
    std::cout << std::fixed;
    std::cout << "Number of workers : " << n_workers << "\n";

    Graph g;
    std::cout << "Reading graph\n";
    g.readGraphFromBinary<int>(input_file_path);
    std::cout << "Created graph\n";

    triangleCountParallel(g, n_workers);

    return 0;
}
