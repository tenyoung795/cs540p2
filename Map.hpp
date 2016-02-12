#ifndef MAP_HPP
#define MAP_HPP

#include <cstddef>

#include <algorithm>
#include <array>
#include <iterator>
#include <list>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace cs540 {
namespace {
constexpr std::size_t MAX_HEIGHT = 32;

template <typename>
struct Node;

template <typename V>
using NodeIterator = typename std::list<Node<V>>::iterator;

template <typename V>
struct Node {
    struct Link {
        NodeIterator<V> prev;
        NodeIterator<V> next;
    };

    V value;
    std::size_t height;
    std::array<Link, MAX_HEIGHT> levels;

    template <typename T>
    Node(T &&value, std::size_t height) : value{std::forward<T>(value)}, height{height} {}
};

template <typename I>
class ValueIterator : public std::iterator<
    std::bidirectional_iterator_tag,
    decltype(std::declval<typename std::iterator_traits<I>::value_type>().value),
    typename std::iterator_traits<I>::difference_type,
    decltype(&typename std::iterator_traits<I>::pointer {}->value),
    decltype((std::declval<typename std::iterator_traits<I>::reference>().value))
> {
    static_assert(std::is_base_of<
        std::bidirectional_iterator_tag,
        typename std::iterator_traits<I>::iterator_category
    >::value, "I must be bidirectional");

    I _iter;

    template <typename I1, typename I2>
    friend bool operator==(const ValueIterator<I1> &, const ValueIterator<I2> &);

protected:
    ValueIterator(const I &iter): _iter{iter} {}

    constexpr const I &get() const {
        return _iter;
    }

    template <typename, typename>
    friend class Map;

public:
    ValueIterator &operator++() {
        ++_iter;
        return *this;
    }

    ValueIterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    ValueIterator &operator--() {
        --_iter;
        return *this;
    }

    ValueIterator operator--(int) {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    auto &operator*() const {
        return _iter->value;
    }

    auto *operator->() const {
        return &**this;
    }
};

template <typename I1, typename I2>
bool operator==(const ValueIterator<I1> &i1, const ValueIterator<I2> &i2) {
    return i1._iter == i2._iter;
}

template <typename I1, typename I2>
bool operator!=(const ValueIterator<I1> &i1, const ValueIterator<I2> &i2) {
    return !(i1 == i2);
}

// Wrapper around std::size_t that automatically zeros upon move.
class Height {
    std::size_t _value;

public:
    constexpr Height(): _value{} {}

    constexpr Height(const Height &that): _value{that._value} {}

    constexpr Height(Height &&that): _value{that} {
        that._value = 0;
    }

    constexpr Height &operator=(const Height &that) {
        if (this != &that) {
            _value = that._value;
        }
        return *this;
    }

    constexpr Height &operator=(Height &&that) {
        _value = that._value;
        that._value = 0;
        return *this;
    }

    constexpr operator const std::size_t &() const {
        return _value;
    }

    constexpr operator std::size_t &() {
        return _value;
    }
};
} // anonymous namespace

template <typename K, typename M>
class Map {
public:
    using ValueType = std::pair<const K, M>;

private:
    using _List = std::list<Node<ValueType>>;

public:
    using Iterator = ValueIterator<typename _List::iterator>;
    using ConstIterator = ValueIterator<typename _List::const_iterator>;
    using ReverseIterator = ValueIterator<typename _List::reverse_iterator>;

private:
    std::list<Node<ValueType>> _nodes;
    Height _height;
    std::default_random_engine _random;

    template <typename This>
    static auto _lower_bound(This &&self, const K &key) {
        auto end = self.end();
        auto iter = std::lower_bound(self.begin(), end, key, [](auto &value, auto &key) {
            return value.first < key;
        });
        return std::make_pair(iter, iter != end && iter->first == key);
    }

    template <typename This>
    static auto _find(This &&self, const K &key) {
        auto result = _lower_bound(self, key);
        return result.second ? result.first : self.end();
    }

    template <typename This>
    static auto &_at(This &&self, const K &key) {
        auto iter = self.find(key);
        if (iter == self.end()) {
            throw std::out_of_range{"Not found"};
        }
        return iter->second;
    }

    template <typename V>
    Iterator _insert_before(Iterator iter, V &&value) {
        Height height;
        std::bernoulli_distribution flip_coin;
        for (; height < MAX_HEIGHT && flip_coin(_random); ++height);
        _height = std::max(_height, height);
        return _nodes.emplace(iter.get(),
                              std::forward<V>(value), height);
    }

    template <typename Key>
    auto &_index(Key &&key) {
        static_assert(std::is_default_constructible<M>::value,
                      "Mapped type must be default constructible");
        auto result = _lower_bound(*this, key);
        return (result.second
            ? result.first
            : _insert_before(
                std::move(result.first),
                std::make_pair(std::forward<Key>(key), M{}))
        )->second;
    }

    template <class V>
    std::pair<Iterator, bool> _insert(V &&value) {
        auto result = _lower_bound(*this, value.first);
        return result.second
            ? std::make_pair(result.first, false)
            : std::make_pair(
                _insert_before(std::move(result.first), std::forward<V>(value)),
                true);
    }

    template <typename This>
    static ValueType &_index(This &&self, std::size_t i) {
        throw std::out_of_range{
            std::to_string(i) + " beyond size " + std::to_string(self.size())};
    }

public:
    Map() : _nodes{}, _height{}, _random{std::random_device {}()} {}

    Map(const Map &that) : Map{} {
        insert(that.begin(), that.end());
    }

    Map(Map &&that) = default;

    Map(std::initializer_list<ValueType> pairs) : Map{} {
        insert(pairs.begin(), pairs.end());
    }

    Map &operator=(const Map &that) {
        if (this != &that) {
            clear();
            insert(that.begin(), that.end());
        }
        return *this;
    }

    Map &operator=(Map &&that) = default;

    ~Map() = default;

    std::size_t size() const {
        return _nodes.size();
    }

    bool empty() const {
        return _nodes.empty();
    }

    Iterator begin() {
        return _nodes.begin();
    }

    Iterator end() {
        return _nodes.end();
    }

    ConstIterator begin() const {
        return _nodes.begin();
    }

    ConstIterator end() const {
        return _nodes.end();
    }

    ReverseIterator rbegin() {
        return _nodes.rbegin();
    }

    ReverseIterator rend() {
        return _nodes.rend();
    }

    Iterator find(const K &key) {
        return _find(*this, key);
    }

    ConstIterator find(const K &key) const {
        return _find(*this, key);
    }

    M &at(const K &key) {
        return _at(*this, key);
    }

    const M &at(const K &key) const {
        return _at(*this, key);
    }

    M &operator[](const K &key) {
        return _index(key);
    }

    M &operator[](K &&key) {
        return _index(std::move(key));
    }

    std::pair<Iterator, bool> insert(const ValueType &value) {
        return _insert(value);
    }

    std::pair<Iterator, bool> insert(ValueType &&value) {
        return _insert(std::move(value));
    }

    template <typename I>
    void insert(I begin, I end) {
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }

    void erase(Iterator iter) {
        _nodes.erase(iter.get());
    }

    void erase(const K &key) {
        auto iter = find(key);
        if (iter == end()) {
            throw std::out_of_range{"Not found"};
        }
        erase(iter);
    }

    void clear() {
        _nodes.clear();
        _height = Height{};
    }

    ValueType &index(std::size_t i) {
        return _index(*this, i);
    }

    const ValueType &index(std::size_t i) const {
        return _index(*this, i);
    }
}; // template <typename, typename> class Map

template <typename K, typename M>
static bool operator==(const Map<K, M> &m1, const Map<K, M> &m2) {
    return m1.size() == m2.size() && std::equal(m1.begin(), m1.end(), m2.begin());
}

template <typename K, typename M>
static bool operator!=(const Map<K, M> &m1, const Map<K, M> &m2) {
    return !(m1 == m2);
}

template <typename K, typename M>
static bool operator<(const Map<K, M> &m1, const Map<K, M> &m2) {
    return std::lexicographical_compare(m1.begin(), m1.end(), m2.begin(), m2.end());
}
} // namespace cs540

#endif // MAP_HPP
