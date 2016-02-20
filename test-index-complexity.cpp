#include <cmath>
#include <cstddef>

#include <chrono>
#include <iostream>
#include <random>
#include <ratio>

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

Duration mean_duration(std::size_t size) {
    cs540::Map<std::size_t, volatile bool> map;
    std::vector<std::size_t> lookup;
    for (std::size_t i = 0; i < size; ++i) {
        map.insert({i, false});
        lookup.push_back(i);
    }
    std::shuffle(lookup.begin(), lookup.end(),
                 std::default_random_engine {std::random_device{} ()});

    accumulator_set<double, stats<tag::mean>> durations;
    for (auto i : lookup) {
        auto start = steady_clock::now();
        map.index(i)->second = true;
        auto duration = steady_clock::now() - start;
        durations(duration.count());
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
    accumulator_set<double, stats<tag::mean, tag::variance>> ns, lg_ns, ks;
    accumulator_set<double, stats<tag::sum>> k_ns, k_lg_ns;
    for (std::size_t i = 0; i < N; ++i) {
        auto n = 8192 << i;
        std::cout << n << " elements\n";
        ns(n);
        auto lg_n = std::log2(n);
        lg_ns(lg_n);
        auto k = mean_duration(n);
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
