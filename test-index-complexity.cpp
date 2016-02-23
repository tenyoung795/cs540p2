#include <cmath>
#include <cstddef>

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <random>
#include <ratio>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "Map.hpp"

using std::chrono::duration;
using std::chrono::steady_clock;

using namespace boost::accumulators;

using Duration = duration<double, steady_clock::period>;

constexpr std::size_t STRIDE_LENGTH = 8192;

Duration mean_duration(std::size_t num_strides) {
    auto size = num_strides * STRIDE_LENGTH;

    cs540::Map<std::size_t, volatile bool> map;
    std::vector<std::size_t> lookup;
    lookup.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        map.insert({i, false});
        lookup.push_back(i);
    }
    std::shuffle(lookup.begin(), lookup.end(),
                 std::default_random_engine {std::random_device{} ()});

    std::vector<std::future<Duration>> futures;
    futures.reserve(num_strides);
    for (std::size_t i = 0; i < num_strides; ++i) {
        futures.push_back(std::async([&map, &lookup, i] {
            accumulator_set<double, stats<tag::mean>> durations;
             for (std::size_t j = 0; j < STRIDE_LENGTH; ++j) {
                auto start = steady_clock::now();
                map.index(lookup[i * STRIDE_LENGTH + j])->second = true;
                auto duration = steady_clock::now() - start;
                durations(duration.count());
            }
            return Duration {mean(durations)};
        }));
    }

    accumulator_set<double, stats<tag::mean>> durations;
    for (auto &future : futures) {
        durations(future.get().count());
    }
    return Duration {mean(durations)};
}

template <typename Xs, typename Ys, typename XYs>
auto correlation(std::size_t n, Xs &&xs, Ys &&ys, XYs &&xys) {
    auto numerator = sum(xys) - n * mean(xs) * mean(ys);
    auto denominator = n * std::sqrt(variance(xs) * variance(ys));
    return denominator == 0
        ? (numerator > 0 ? 1 : (numerator < 0 ? -1 : 0))
        : numerator / denominator;
}

int main() {
    constexpr std::size_t N = 4;
    std::vector<std::future<Duration>> futures;
    futures.reserve(N);
    accumulator_set<double, stats<tag::mean, tag::variance>> ns, lg_ns, ks;
    accumulator_set<double, stats<tag::sum>> k_ns, k_lg_ns;
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t num_strides = 1 << i;
        futures.push_back(std::async(mean_duration, num_strides));
    }
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t num_strides = 1 << i;
        auto n = num_strides * STRIDE_LENGTH;
        ns(n);
        auto lg_n = std::log2(n);
        lg_ns(lg_n);

        std::cout << n << " elements:\n";
        auto k = futures[i].get();
        auto us = std::chrono::duration_cast<duration<double, std::micro>>(k);
        std::cout << '\t' << us.count() << " μs\n";
        ks(k.count());
        k_ns(k.count() * n);
        k_lg_ns(k.count() * lg_n);
    }
    auto n_correlation = correlation(N, ns, ks, k_ns);
    auto lg_n_correlation = correlation(N, lg_ns, ks, k_lg_ns);
    std::cout << "Correlation bewteen N and duration is " << n_correlation << '\n';
    std::cout << "Correlation bewteen lg N and duration is " << lg_n_correlation << '\n';
    if (lg_n_correlation <= n_correlation) {
        return EXIT_FAILURE;
    }
}
