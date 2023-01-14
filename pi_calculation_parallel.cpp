#include "core/utils.h"
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <atomic>
#include <random>
#include <vector>
#include <functional>

#define sqr(x) ((x) * (x))
#define DEFAULT_NUMBER_OF_POINTS "12345678"

using std::thread;
using std::atomic;
using std::random_device;
using std::vector;
using std::ref;

typedef struct {
    uint total_num_points;
    uint num_circle_points;
    double time_taken;
} Result;

const uint c_const = (uint)RAND_MAX + (uint)1;
inline double get_random_coordinate(uint *random_seed)
{
    return ((double)rand_r(random_seed)) / c_const;
}

uint get_points_in_circle(uint m, uint random_seed)
{
    uint circle_count = 0;
    for (uint i = 0; i < m; i++) {
        double x_coord = (2.0 * get_random_coordinate(&random_seed)) - 1.0;
        double y_coord = (2.0 * get_random_coordinate(&random_seed)) - 1.0;
        if ((sqr(x_coord) + sqr(y_coord)) <= 1.0) {
            circle_count++;
        }
    }
    return circle_count;
}

// Pass this function in during thread creation.
void add_points(atomic<uint>& global_circle_count,
                uint m,
                Result* thread_results,
                uint thread_index)
{
    timer points_timer;
    points_timer.start();
    random_device rand_dev;
    uint num_circle_points = get_points_in_circle(m, rand_dev());
    global_circle_count += num_circle_points;
    double time_taken = points_timer.stop();

    Result res = {m, num_circle_points, time_taken};
    thread_results[thread_index] = res;
}

void piCalculation(uint n, uint t)
{
    timer serial_timer;
    double time_taken = 0.0;

    atomic<uint> circle_points(0);
    vector<thread> threads;
    Result* p_thread_results = new Result[t];

    serial_timer.start();
    // Create threads and distribute the work across T threads
    // ------------------------------------------------------------------
    uint quotient = n / t;
    uint remainder = n % t;
    uint num_points_for_final_thread = quotient + remainder;
    for (uint i = 0; i < t; i++) {
        uint m = (i == t - 1) ? num_points_for_final_thread : quotient;
        threads.push_back(thread(add_points, ref(circle_points), m, p_thread_results, i));
    }
    for (auto& lwp : threads) {
        lwp.join();
    }
    // -------------------------------------------------------------------
    time_taken = serial_timer.stop();

    std::cout << "thread_id, points_generated, circle_points, time_taken\n";
    // Print the above statistics for each thread
    // Example output for 2 threads:
    // thread_id, points_generated, circle_points, time_taken
    // 1, 100, 90, 0.12
    // 0, 100, 89, 0.12
    for (uint i = 0; i < t; i++) {
        Result res = p_thread_results[i];
        std::cout << i << ", " << res.total_num_points << ", " << res.num_circle_points << ", " << res.time_taken << "\n";
    }

    // Print the overall statistics
    double pi_value = 4.0 * (circle_points / (double) n);
    std::cout << "Total points generated : " << n << "\n";
    std::cout << "Total points in circle : " << circle_points << "\n";
    std::cout << "Result : " << std::setprecision(VAL_PRECISION) << pi_value
                << "\n";
    std::cout << "Time taken (in seconds) : " << std::setprecision(TIME_PRECISION)
                << time_taken << "\n";

    delete[] p_thread_results;
}

int main(int argc, char *argv[])
{
    // Initialize command line arguments
    cxxopts::Options options("pi_calculation", "Calculate pi using serial and parallel execution");
    options.add_options(
        "custom",
        {
            {"nPoints", "Number of points",
            cxxopts::value<uint>()->default_value(DEFAULT_NUMBER_OF_POINTS)},
            {"nWorkers", "Number of workers",
            cxxopts::value<uint>()->default_value(DEFAULT_NUMBER_OF_WORKERS)},
        }
    );

    auto cl_options = options.parse(argc, argv);
    uint n_points = cl_options["nPoints"].as<uint>();
    uint n_workers = cl_options["nWorkers"].as<uint>();
    std::cout << std::fixed;
    std::cout << "Number of points : " << n_points << "\n";
    std::cout << "Number of workers : " << n_workers << "\n";

    piCalculation(n_points, n_workers);

    return 0;
}

