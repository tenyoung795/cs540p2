#ifndef MAP_HPP
#define MAP_HPP

#include <cstddef>

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace cs540 {
namespace {
template <typename V>
class Node {
    struct _Deleter {
        void operator()(Node *node) const {
            node->~Node();
            delete[] reinterpret_cast<char *>(node);
        }
    };

public:
    using UniquePtr = std::unique_ptr<Node, _Deleter>;

private:
    template <typename T, typename Iter>
    Node(T &&v, std::size_t h, UniquePtr &&n, Node *p, Iter iter)
        : value{std::forward<T>(v)}, height{h}, next{std::move(n)}, prev{p} {
        for (std::size_t i = 0; i < height; ++i) {
            Link &link = levels()[i];
            auto prev = *iter;
            auto next = prev ? prev->levels()[i].next : nullptr;
            link = {prev, next};
            (prev ? prev->levels()[i].next : *iter) = this;
            if (next) {
                next->levels()[i].prev = this;
            }
            ++iter;
        }
        if (next) {
            next->prev = this;
        }
        // Caller (i.e. Node<V>::make) must connect prev properly
    }

public:
    struct Link {
        Node *prev;
        Node *next;
    };

    V value;
    const std::size_t height;
    UniquePtr next;
    Node *prev;
    // Link levels[height];

    template <typename T, typename Iter>
    static UniquePtr make(T &&value, std::size_t height, UniquePtr &&next, Node *prev,
                          Iter iter) {
        auto size = sizeof(Node) + height * sizeof(Link);
        auto ptr = new char[size];
        UniquePtr result{
            new(ptr) Node{std::forward<T>(value), height, std::move(next), prev, iter}
        };
        if (prev) {
            prev->next = std::move(result);
        }
        return result;
    }

    Link *levels() {
        return reinterpret_cast<Link *>(
            reinterpret_cast<char *>(this) + sizeof(*this));
    }

    const Link *levels() const {
        return reinterpret_cast<const Link *>(
            reinterpret_cast<const char *>(this) + sizeof(*this));
    }
};

constexpr struct {} construct_end{};

template <typename V>
class Iter : public std::iterator<std::bidirectional_iterator_tag, V> {
    Node<V> *_prev;
    Node<V> *_node;
    Node<V> *_next;

    template <typename T>
    friend bool operator==(const Iter<T> &, const Iter<T> &);

protected:
    explicit Iter(Node<V> *node)
        : _prev{node ? node->prev : nullptr},
          _node{node},
          _next{node ? node->next.get() : nullptr} {}

    explicit Iter(decltype(construct_end), Node<V> *last)
        : _prev{last}, _node{nullptr}, _next{nullptr} {}

    Node<V> *prev() && {
        return _prev;
    }

    Node<V> *node() && {
        return _node;
    }

    template <typename, typename>
    friend class Map;

public:
    Iter() = delete;
    Iter(const Iter &) = default;
    ~Iter() = default;
    Iter &operator=(const Iter &) = default;

    Iter &operator++() {
        _prev = _node;
        _node = _next;
        _next = _next ? _next->next.get() : nullptr;
        return *this;
    }

    Iter operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    Iter &operator--() {
        _next = _node;
        _node = _prev;
        _prev = _prev ? _prev->prev : nullptr;
        return *this;
    }

    Iter operator--(int) {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    V &operator*() const {
        return _node->value;
    }

    V *operator->() const {
        return &**this;
    }
};

template <typename V>
class ConstIter : public std::iterator<std::bidirectional_iterator_tag, const V> {
    Iter<V> _iter;

    template <typename T>
    friend bool operator==(const ConstIter<T> &, const ConstIter<T> &);

public:
    ConstIter() = delete;
    ConstIter(const ConstIter &) = default;
    ConstIter(const Iter<V> &iter) : _iter{iter} {}
    ~ConstIter() = default;
    ConstIter &operator=(const ConstIter &) = default;

    ConstIter &operator++() {
        ++_iter;
        return *this;
    }

    ConstIter operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    ConstIter &operator--() {
        --_iter;
        return *this;
    }

    ConstIter operator--(int) {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    const V &operator*() const {
        return *_iter;
    }

    const V *operator->() const {
        return &**this;
    }
};

template <typename V>
bool operator==(const Iter<V> &i1, const Iter<V> &i2) {
    return i1._node == i2._node;
}

template <typename V>
bool operator==(const ConstIter<V> &i1, const ConstIter<V> &i2) {
    return i1._iter == i2._iter;
}

template <typename V>
bool operator==(const Iter<V> &i1, const ConstIter<V> &i2) {
    return ConstIter<V>{i1} == i2;
}

template <typename V>
bool operator==(const ConstIter<V> &i1, const Iter<V> &i2) {
    return i2 == i1;
}

template <typename V>
bool operator!=(const Iter<V> &i1, const Iter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
bool operator!=(const ConstIter<V> &i1, const ConstIter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
bool operator!=(const Iter<V> &i1, const ConstIter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
bool operator!=(const ConstIter<V> &i1, const Iter<V> &i2) {
    return !(i1 == i2);
}
} // anonymous namespace

template <typename K, typename M>
class Map {
public:
    using ValueType = std::pair<const K, M>;
    using Iterator = Iter<ValueType>;
    using ConstIterator = ConstIter<ValueType>;
    using ReverseIterator = std::reverse_iterator<Iterator>;

private:
    static constexpr auto MAX_HEIGHT = 32;

    typename Node<ValueType>::UniquePtr _head;
    std::array<Node<ValueType> *, MAX_HEIGHT> _levels;
    Node<ValueType> *_last;
    std::size_t _size;
    std::size_t _height;
    std::default_random_engine _random;

    Iterator _begin() const {
        return Iterator {_head.get()};
    }

    Iterator _end() const {
        return Iterator {construct_end, _last};
    }

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
        std::size_t height = 0;
        std::bernoulli_distribution flip_coin;
        for (; height < MAX_HEIGHT && flip_coin(_random); ++height);
        _height = std::max(_height, height);

        auto prev = std::move(iter).prev();
        auto &next = prev ? prev->next : _head;
        auto result = Node<ValueType>::make(
            std::forward<V>(value), height, std::move(next), prev,
            _levels.begin());
        ++_size;

        if (prev) {
            auto &node = *prev->next;
            if (!node.next) {
                _last = &node;
            }
            return Iterator {&node};
        }

        // We've inserted the head
        if (!_last) {
            _last = &*result;
        }
        _head = std::move(result);
        return begin();
    }

    template <typename Key>
    auto &_index(Key &&key) {
        static_assert(std::is_default_constructible<M>::value,
                      "Mapped type must be default constructible");
        auto &&result = _lower_bound(*this, key);
        return (result.second
            ? result.first
            : _insert_before(
                result.first,
                std::make_pair(std::forward<Key>(key), M{}))
        )->second;
    }

    template <class V>
    std::pair<Iterator, bool> _insert(V &&value) {
        auto &&result = _lower_bound(*this, value.first);
        return result.second
            ? std::make_pair(result.first, false)
            : std::make_pair(
                _insert_before(result.first, std::forward<V>(value)),
                true);
    }

    template <typename This>
    static ValueType &_index(This &&self, std::size_t i) {
        throw std::out_of_range{
            std::to_string(i) + " beyond size " + std::to_string(self.size())};
    }

public:
    Map() :
        _head{}, _levels{},
        _last{}, _size{}, _height{},
        _random{std::random_device {}()} {}

    Map(const Map &that) : Map{} {
        insert(that.begin(), that.end());
    }

    Map(Map &&that)
        : _head{std::move(that._head)}, _levels{that._levels},
          _last{that._last}, _size{that._size}, _height{that._height},
          _random{that._random} {
        that.clear();
    }

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

    Map &operator=(Map &&that) {
        _head = std::move(that._head);
        _levels = that._levels;
        _last = that._last;
        _size = that._size;
        _height = that._height;
        _random = that._random;
        that.clear();
        return *this;
    }

    ~Map() = default;

    constexpr std::size_t size() const {
        return _size;
    }

    constexpr bool empty() const {
        return size() == 0;
    }

    Iterator begin() {
        return _begin();
    }

    Iterator end() {
        return _end();
    }

    ConstIterator begin() const {
        return _begin();
    }

    ConstIterator end() const {
        return _end();
    }

    ReverseIterator rbegin() {
        return std::make_reverse_iterator(end());
    }

    ReverseIterator rend() {
        return std::make_reverse_iterator(begin());
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
        auto node = std::move(iter).node();
        auto levels = node->levels();
        for (std::size_t i = 0; i < node->height; ++i) {
            auto &l = levels[i];
            if (l.next) {
                l.next->levels()[i].prev = l.prev;
            }
            (l.prev ? l.prev->levels()[i].next : _levels[i]) = l.next;
        }
        if (node->height == _height) {
            // We may have decreased the height
            for (; _height > 0 && !_levels[_height - 1]; --_height);
        }
        (node->next ? node->next->prev : _last) = node->prev;
        (node->prev ? node->prev->next : _head) = std::move(node->next);
        --_size;
    }

    void erase(const K &key) {
        auto iter = find(key);
        if (iter == end()) {
            throw std::out_of_range{"Not found"};
        }
        erase(iter);
    }

    void clear() {
        _head = nullptr;
        _levels.fill(nullptr);
        _last = nullptr;
        _size = 0;
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
