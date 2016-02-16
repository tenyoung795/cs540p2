#ifndef MAP_HPP
#define MAP_HPP

#include <cassert>
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
struct Link;

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

    V value;
    const std::size_t height;
    Node *prev;
    UniquePtr next;
    // Link<V> levels[height];

    Link<V> *levels() {
        return reinterpret_cast<Link<V> *>(
            reinterpret_cast<char *>(this) + _PADDING + sizeof(*this));
    }

    const Link<V> *levels() const {
        return reinterpret_cast<const Link<V> *>(
            reinterpret_cast<const char *>(this) + _PADDING + sizeof(*this));
    }

protected:
    template <typename T, typename Iter>
    static UniquePtr make(T &&value, std::size_t height, Node *prev, UniquePtr &&next,
                          Iter iter) {
        auto size = sizeof(Node) + _PADDING + height * sizeof(Link<V>);
        auto ptr = new char[size];
        UniquePtr result{
            new(ptr) Node{std::forward<T>(value), height, prev, std::move(next), iter}
        };
        if (prev) {
            prev->next = std::move(result);
        }
        assert(!prev == !!result);
        return result;
    }

    template <typename, typename>
    friend class Map;

private:
    static constexpr auto _PADDING = alignof(Link<V>) > alignof(Node)
        ? alignof(Link<V>) - alignof(Node)
        : 0;

    template <typename T, typename Iter>
    Node(T &&v, std::size_t h, Node *p, UniquePtr &&n, Iter iter)
        : value{std::forward<T>(v)}, height{h}, prev{p}, next{std::move(n)} {
        if (next) {
            next->prev = this;
        }
        for (std::size_t i = 0; i < height; ++i) {
            auto &level = levels()[i];
            new(&level) auto(*iter);
            if (level.prev) {
                level.prev->levels()[i].next = this;
            }
            if (level.next) {
                level.next->levels()[i].prev = this;
            }
            ++iter;
        }
        // Caller (i.e. Node<V>::make) must connect prev properly
    }

    ~Node() {
        while (next) {
            next = std::move(next->next);
        }
    }
};

template <typename V>
struct Link {
    Node<V> *prev;
    Node<V> *next;
};

constexpr struct {} construct_end{};

template <typename V>
class Iter : public std::iterator<std::bidirectional_iterator_tag, V> {
    Node<V> *_prev;
    Node<V> *_node;
    Node<V> *_next;

    template <typename T>
    friend constexpr bool operator==(const Iter<T> &, const Iter<T> &);

protected:
    constexpr explicit Iter(Node<V> *node)
        : _prev{node ? node->prev : nullptr},
          _node{node},
          _next{node ? node->next.get() : nullptr} {}

    constexpr explicit Iter(decltype(construct_end), Node<V> *last)
        : _prev{last}, _node{nullptr}, _next{nullptr} {}

    constexpr Node<V> *prev() {
        return _prev;
    }

    constexpr Node<V> *node() {
        return _node;
    }

    template <typename, typename>
    friend class Map;

public:
    constexpr Iter() : Iter{construct_end, nullptr} {}
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
    friend constexpr bool operator==(const ConstIter<T> &, const ConstIter<T> &);

public:
    constexpr ConstIter() = default;
    ConstIter(const ConstIter &) = default;
    constexpr ConstIter(const Iter<V> &iter) : _iter{iter} {}
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
constexpr bool operator==(const Iter<V> &i1, const Iter<V> &i2) {
    return i1._node == i2._node;
}

template <typename V>
constexpr bool operator==(const ConstIter<V> &i1, const ConstIter<V> &i2) {
    return i1._iter == i2._iter;
}

template <typename V>
constexpr bool operator==(const Iter<V> &i1, const ConstIter<V> &i2) {
    return ConstIter<V>{i1} == i2;
}

template <typename V>
constexpr bool operator==(const ConstIter<V> &i1, const Iter<V> &i2) {
    return i2 == i1;
}

template <typename V>
constexpr bool operator!=(const Iter<V> &i1, const Iter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
constexpr bool operator!=(const ConstIter<V> &i1, const ConstIter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
constexpr bool operator!=(const Iter<V> &i1, const ConstIter<V> &i2) {
    return !(i1 == i2);
}

template <typename V>
constexpr bool operator!=(const ConstIter<V> &i1, const Iter<V> &i2) {
    return !(i1 == i2);
}

// Wrapper that defaults on move
template <typename T>
struct DefaultOnMove {
    static_assert(std::is_default_constructible<T>::value,
                  "T must be default constructible");
    static_assert(std::is_trivially_move_constructible<T>::value,
                  "T must be trivially move constructible");
    static_assert(std::is_trivially_move_assignable<T>::value,
                  "T must be trivially move assignable");

    T value;

    DefaultOnMove() : value{} {}
    DefaultOnMove(const DefaultOnMove &) = default;
    DefaultOnMove(T &&value) : value{std::move(value)} {}
    DefaultOnMove(DefaultOnMove &&that) : value{std::move(that.value)} {
        that.value = {};
    }

    DefaultOnMove &operator=(const DefaultOnMove &) = default;
    DefaultOnMove &operator=(DefaultOnMove &&that) {
        value = std::move(that.value);
        that.value = {};
        return *this;
    }

    constexpr operator T &() {
        return value;
    }

    constexpr operator const T &() const {
        return value;
    }

    operator T () && {
        return std::move(value);
    }
};
} // anonymous namespace

template <typename K, typename M>
class Map {
public:
    using ValueType = std::pair<const K, M>;
    using Iterator = Iter<ValueType>;
    using ConstIterator = ConstIter<ValueType>;
    using ReverseIterator = std::reverse_iterator<Iterator>;

private:
    static constexpr auto MAX_HEIGHT = 31;
    using _Links = std::array<Link<ValueType>, MAX_HEIGHT>;
    class _SearchResult {
        Iterator _iter;
        _Links _links;
        bool _found;

    public:
        explicit _SearchResult(Iterator iter) : _iter{iter}, _found{true} {}

        explicit _SearchResult(Iterator iter, _Links links)
            : _iter{iter}, _links{links}, _found{false} {}

        constexpr const Iterator &iter() const {
            return _iter;
        }

        template <typename F, typename G>
        auto match(F &&found, G &&missing) const {
            return _found
                ? std::forward<F>(found)(_iter)
                : std::forward<G>(missing)(_iter, _links.begin());
        }
    };

    typename Node<ValueType>::UniquePtr _head;
    std::array<Node<ValueType> *, MAX_HEIGHT> _level_heads;
    DefaultOnMove<Node<ValueType> *> _last;
    DefaultOnMove<std::size_t> _size;
    DefaultOnMove<std::size_t> _height;
    std::default_random_engine _random;
    std::bernoulli_distribution _flip_coin;

    Iterator _begin() const {
        return Iterator {_head.get()};
    }

    Iterator _end() const {
        return Iterator {construct_end, _last};
    }

    template <typename Key>
    _SearchResult _lower_bound(const Key &key) const {
        _Links links{};
        for (auto i = _height; i > 0; --i) {
            auto &link = links[i - 1];
            link = i == _height || !links[i].prev
                ? Link<ValueType> {nullptr, _level_heads[i - 1]}
                : Link<ValueType> {links[i].prev, links[i].prev->levels()[i - 1].next};
            while (link.next && link.next->value.first < key) {
                if (link.next->value.first == key) {
                    return _SearchResult {Iterator {link.next}};
                }
                link = {link.next, link.next->levels()[i - 1].next};
            }
        }
        auto begin = _height > 0 && links[0].prev
            ? Iterator {links[0].prev}
            : _begin();
        auto end = _end();
        auto result = std::find_if_not(begin, end, [&] (auto &value) {
            return value.first < key;
        });
        if (result != end && result->first == key) {
            return _SearchResult {result};
        }
        return _SearchResult {result, links};
    }

    template <typename Key>
    Iterator _find(const Key &key) const {
        return _lower_bound(key).match([] (auto iter) {
            return iter;
        }, [this] (auto, auto) {
            return this->_end();
        });
    }

    template <typename This, typename Key>
    static auto &_at(This &&self, const Key &key) {
        auto iter = self.find(key);
        if (iter == self.end()) {
            throw std::out_of_range{"Not found"};
        }
        return iter->second;
    }

    template <typename LinkIter, typename V>
    Iterator _insert_before(Iterator iter, LinkIter link_iter, V &&value) {
        DefaultOnMove<std::size_t> height;
        for (; height < MAX_HEIGHT && _flip_coin(_random); ++height);
        _height = std::max(_height, height);

        auto prev = iter.prev();
        auto &next = prev ? prev->next : _head;
        auto result = Node<ValueType>::make(
            std::forward<V>(value), height, prev, std::move(next),
            link_iter);
        auto &node = prev ? *prev->next : *result;

        for (std::size_t i = 0; i < height; ++i) {
            if (!link_iter->prev) _level_heads[i] = &node;
            ++link_iter;
        }

        if (!node.next) {
            _last = &node;
        }
        if (!node.prev) {
            _head = std::move(result);
        }

        ++_size;
        return Iterator {&node};
    }

    template <typename Key>
    auto &_index(Key &&key) {
        static_assert(std::is_default_constructible<M>::value,
                      "Mapped type must be default constructible");
        return _lower_bound(key).match([] (auto iter) {
            return iter;
        }, [&, this] (auto iter, auto link_iter) {
            return this->_insert_before(
                iter, link_iter,
                std::make_pair(std::forward<Key>(key), M{}));
        })->second;
    }

    template <class V>
    std::pair<Iterator, bool> _insert(V &&value) {
        return _lower_bound(value.first).match([] (auto iter) {
            return std::make_pair(iter, false);
        }, [&, this] (auto iter, auto link_iter) {
            return std::make_pair(
                this->_insert_before(iter, link_iter, std::forward<V>(value)),
                true);
        });
    }

public:
    Map() :
        _head{},
        _last{}, _size{}, _height{},
        _random{std::random_device {}()}, _flip_coin{} {}

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

    template <typename Key>
    Iterator find(const Key &key) {
        return _find(key);
    }

    template <typename Key>
    ConstIterator find(const Key &key) const {
        return _find(key);
    }

    template <typename Key>
    M &at(const Key &key) {
        return _at(*this, key);
    }

    template <typename Key>
    const M &at(const Key &key) const {
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
        auto node = iter.node();
        auto levels = node->levels();
        for (std::size_t i = 0; i < node->height; ++i) {
            auto &l = levels[i];
            if (l.next) {
                l.next->levels()[i].prev = l.prev;
            }
            (l.prev ? l.prev->levels()[i].next : _level_heads[i]) = l.next;
        }
        if (node->height == _height) {
            // We may have decreased the height
            for (; _height > 0 && !_level_heads[_height - 1]; --_height);
        }
        (node->next ? node->next->prev : _last) = node->prev;
        (node->prev ? node->prev->next : _head) = std::move(node->next);
        --_size;
    }

    template <typename Key>
    void erase(const Key &key) {
        auto iter = find(key);
        if (iter == end()) {
            throw std::out_of_range{"Not found"};
        }
        erase(iter);
    }

    void clear() {
        _head = nullptr;
        _last = nullptr;
        _size = 0;
        _height = 0;
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
