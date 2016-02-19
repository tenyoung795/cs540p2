#include <cstddef>

#include <iterator>
#include <map>
#include <ostream>
#include <stdexcept>
#include <utility>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/irange.hpp>
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>

#include "Map.hpp"

struct unit {};

namespace boost {
namespace test_tools {
template <typename T, typename U>
struct print_log_value<std::pair<T, U>> {
    void operator()(std::ostream &out, const std::pair<T, U> &pair) const {
        out << '(' << pair.first << ", " << pair.second << ')';
    }
};
}
}

template <typename K, typename M>
struct OutOfRangeTest {
    cs540::Map<K, M> map;
    std::size_t offset;
};

template <typename K, typename M>
OutOfRangeTest<K, M> out_of_range_test(cs540::Map<K, M> map, std::size_t offset) {
    return OutOfRangeTest<K, M> {std::move(map), offset};
}

template <typename K, typename M>
void throws_out_of_range(const OutOfRangeTest<K, M> &test) {
    BOOST_CHECK_THROW(test.map.index(test.map.size() + test.offset), std::out_of_range);
}

struct InRangeTest {
    std::size_t size;
    std::size_t index;
};

void at_index(const InRangeTest &test) {
    cs540::Map<std::size_t, std::size_t> map;
    for (std::size_t i = 0; i < test.size; ++i) {
        map.insert({i, i});
    }
    decltype(map)::ValueType expected_value{test.index, test.index};
    BOOST_CHECK_EQUAL(expected_value, map.index(test.index));
}

bool init_unit_test() {
    using namespace boost::adaptors;
    using namespace boost::unit_test;

    auto &suite = framework::master_test_suite();
    for (std::size_t size = 0; size < 10; ++size) {
        cs540::Map<std::size_t, unit> map;
        for (std::size_t i = 0; i < size; ++i) {
            map.insert({i, {}});
        }
        auto &&range = boost::irange(0, 10) | transformed([map] (auto offset) {
            return out_of_range_test(std::move(map), offset);
        });
        suite.add(BOOST_PARAM_TEST_CASE((throws_out_of_range<std::size_t, unit>),
                                        range.begin(), range.end()));
    }
    for (std::size_t size = 0; size < 10; ++size) {
        auto &&range = boost::irange(std::size_t {}, size) | transformed([size] (auto index) {
            return InRangeTest {size, index};
        });
        suite.add(BOOST_PARAM_TEST_CASE(at_index, range.begin(), range.end()));
    }
    return true;
}
