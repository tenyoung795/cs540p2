#ifndef MAP_HPP
#define MAP_HPP

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cs540 {
namespace internal {
class Node;
class Link;
}
}

namespace std {
extern template class reference_wrapper<cs540::internal::Node>;
extern template class geometric_distribution<size_t>;
}

namespace cs540 {
namespace internal {
class Node;

class Link {
    std::reference_wrapper<Node> _prev, _next;
    std::size_t _num_skips_next;

public:
    explicit Link(Node &owner, std::size_t num_skips)
        : _prev{owner}, _next{owner}, _num_skips_next{num_skips} {}
    Link(const Link &) = delete;
    Link(Link &&) = default;
    Link &operator=(const Link &) = delete;
    Link &operator=(Link &&) = default;

    Node &prev() {
        return _prev;
    }

    const Node &prev() const {
        return _prev;
    }

    Node &next() {
        return _next;
    }

    const Node &next() const {
        return _next;
    }

    std::size_t num_skips_next() const {
        return _num_skips_next;
    }

    void insert_before(Node &, std::size_t i, std::size_t num_skips);
    void disconnect(std::size_t);
}; // class Link

struct NodeInsertion {
    std::reference_wrapper<Node> ref;
    std::size_t num_skips;
};

class Node {
    std::reference_wrapper<Node> _prev, _next;
    std::vector<Link> _links;

public:
    Node() : _prev{*this}, _next{*this}, _links{} {}

    explicit Node(std::size_t height) : Node{} {
        grow(height, 0);
    }

    Node(const Node &) = delete;
    Node(Node &&) = delete;
    Node &operator=(const Node &) = delete;
    Node &operator=(Node &&) = delete;
    virtual ~Node();

    std::size_t height() const {
        return _links.size();
    }

    Node &prev() {
        return _prev;
    }

    const Node &prev() const {
        return _prev;
    }

    Node &next() {
        return _next;
    }

    const Node &next() const {
        return _next;
    }

    Link &links(std::size_t i) {
        return _links.at(i);
    }

    const Link &links(std::size_t i) const {
        return _links.at(i);
    }

    void grow(std::size_t count, std::size_t width);

    template <typename InsertionIter>
    void insert_before(Node &prev, std::size_t prev_index,
                       Node &sentinel, std::size_t width,
                       InsertionIter levels) noexcept {
        static_assert(std::is_convertible<
            typename std::iterator_traits<InsertionIter>::reference,
            NodeInsertion>::value,
            "InsertionIter's reference type must be convertible to NodeInsertion");
        _prev.get()._next = std::ref(prev);
        prev._prev = _prev;
        prev._next = std::ref(*this);
        _prev = std::ref(prev);
        auto i = prev.height();
        auto last_height = sentinel.height();
        if (i > last_height) {
            sentinel.grow(i - last_height, width);
        }
        for (; i > last_height; --i) {
            sentinel.links(i - 1).insert_before(prev, i - 1, prev_index);
        }
        for (; i > 0; --i) {
            NodeInsertion insertion = *levels;
            insertion.ref.get().links(i - 1).insert_before(
                prev, i - 1, insertion.num_skips);
            ++levels;
        }
    }

    void shrink() noexcept;
}; // class Node
} // namespace internal

namespace {
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
    DefaultOnMove(const T &value) : value{value} {}
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
}; // template <typename> DefaultOnMove

template <bool Const, typename T>
using ConstOrMutT = std::conditional_t<Const, const T, std::remove_const_t<T>>;

template <typename T>
class SelfIndirecting {
    T _t;

public:
    SelfIndirecting(T &&t) : _t{std::move(t)} {}

    T *operator->() {
        return &_t;
    }

    const T *operator->() const {
        return &_t;
    }
};

class SentinelIter : public std::iterator<
    std::input_iterator_tag,
    internal::NodeInsertion,
    void,
    SelfIndirecting<internal::NodeInsertion>,
    internal::NodeInsertion> {
    using Node = internal::Node;
    using NodeInsertion = internal::NodeInsertion;

    std::reference_wrapper<Node> _node;

public:
    explicit SentinelIter(Node &node) : _node{node} {}

    SentinelIter &operator++() {
        return *this;
    }

    NodeInsertion operator*() const {
        return {_node, 0};
    }

    SelfIndirecting<NodeInsertion> operator->() const {
        return **this;
    }

    friend bool operator==(const SentinelIter &i1, const SentinelIter &i2) {
        return &i1._node.get() == &i2._node.get();
    }

    friend bool operator!=(const SentinelIter &i1, const SentinelIter &i2) {
        return !(i1 == i2);
    }
};
} // anonymous namespace

template <typename K, typename M>
class Map {
public:
    using ValueType = std::pair<const K, M>;

private:
    using Node = internal::Node;
    using Link = internal::Link;
    using NodeInsertion = internal::NodeInsertion;

    template <bool Const>
    class _Iter : public std::iterator<
        std::bidirectional_iterator_tag,
        ValueType,
        std::ptrdiff_t,
        ConstOrMutT<Const, ValueType> *,
        ConstOrMutT<Const, ValueType> &>  {
        ConstOrMutT<Const, Node> *_node;

        friend constexpr bool operator==(const _Iter &i1, const _Iter &i2) {
            return i1._node == i2._node;
        }

        friend constexpr bool operator!=(const _Iter &i1, const _Iter &i2) {
            return !(i1 == i2);
        }

    protected:
        constexpr _Iter(ConstOrMutT<Const, Node> &node) : _node{&node} {}

        constexpr ConstOrMutT<Const, Node> &node() const {
            return *_node;
        }

        friend class Map;

    public:
        constexpr _Iter() : _node{} {}
        constexpr _Iter(const _Iter<false> &that) : _node{that._node} {}

        _Iter &operator++() {
            _node = &_node->next();
            return *this;
        }

        _Iter operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        _Iter &operator--() {
            _node = &_node->prev();
            return *this;
        }

        _Iter operator--(int) {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        ConstOrMutT<Const, ValueType> &operator*() const {
            return dynamic_cast<ConstOrMutT<Const, _DereferenceableNode> *>(
                _node)->value;
        }

        ConstOrMutT<Const, ValueType> *operator->() const {
            return &**this;
        }
    }; // template <bool> class _Iter

public:
    using Iterator = _Iter<false>;
    using ConstIterator = _Iter<true>;
    using ReverseIterator = std::reverse_iterator<Iterator>;

private:
    using _NodeInsertions = std::vector<NodeInsertion>;

    struct _DereferenceableNode : Node {
        ValueType value;

        template <typename V>
        _DereferenceableNode(V &&value, std::size_t height)
            : Node{height}, value{std::forward<V>(value)} {}
    };

    class _SearchResult {
        Iterator _iter;
        std::size_t _index;
        _NodeInsertions _insertions;
        bool _found;

    public:
        explicit _SearchResult(Iterator iter) :
            _iter{iter}, _index{}, _insertions{}, _found{true} {}

        explicit _SearchResult(Iterator iter,
                               std::size_t index, _NodeInsertions &&insertions) :
            _iter{iter}, _index{index}, _insertions{std::move(insertions)}, _found{false} {}

        template <typename F, typename G>
        auto match(F &&found, G &&missing) const {
            return _found
                ? std::forward<F>(found)(_iter)
                : std::forward<G>(missing)(_iter, _index, _insertions.begin());
        }
    };

    Node _sentinel;
    std::size_t _size;
    std::default_random_engine _random;
    std::geometric_distribution<std::size_t> _height_generator;

    static Iterator _iter(Node &node) {
        return node;
    }

    static ConstIterator _iter(const Node &node) {
        return node;
    }

    template <typename This>
    static auto _begin(This &&map) {
        return _iter(map._sentinel.next());
    }

    template <typename This>
    static auto _end(This &&map) {
        return _iter(map._sentinel);
    }

    _SearchResult _lower_bound(const K &key) {
        _NodeInsertions insertions;
        insertions.reserve(_sentinel.height());

        if (empty() || Iterator {_sentinel.prev()}->first < key) {
            insertions.insert(
                insertions.end(),
                _sentinel.height(), {std::ref(_sentinel), 0});
            return _SearchResult {end(), _size, std::move(insertions)};
        }

        auto node_ref = std::ref(_sentinel);
        auto index = SIZE_MAX;
        for (auto i = _sentinel.height(); i > 0; --i) {
            while (true) {
                auto &next = node_ref.get().links(i - 1).next();
                Iterator iter = next;
                if (&next == &_sentinel || key < iter->first) break;
                if (iter->first == key) {
                    return _SearchResult {iter};
                }
                index += 1 + node_ref.get().links(i - 1).num_skips_next();
                node_ref = std::ref(next);
            }
            insertions.push_back({node_ref, index});
        }
        for (auto &insertion : insertions) {
            if (insertion.num_skips != SIZE_MAX) {
                assert(index >= insertion.num_skips);
            }
            insertion.num_skips = index - insertion.num_skips;
        }

        Iterator iter = &node_ref.get() == &_sentinel
            ? this->begin()
            : Iterator {insertions.back().ref};
        auto end = this->end();
        if (index == SIZE_MAX) index = 0;
        while (!(iter == end || key < iter->first)) {
            if (key == iter->first) {
                return _SearchResult {iter};
            }
            ++iter;
            ++index;
        }
        return _SearchResult {iter, index, std::move(insertions)};
    }

    template <typename This>
    static auto _find(This &&map, const K &key) {
        using Iter = decltype(map.begin());

        if (map.empty() || _iter(map._sentinel.prev())->first < key) {
            return map.end();
        }

        auto node_ref = std::ref(map._sentinel);
        for (auto i = map._sentinel.height(); i > 0; --i) {
            while (true) {
                auto &next = node_ref.get().links(i - 1).next();
                Iter iter = next;
                if (&next == &map._sentinel || key < iter->first) break;
                if (iter->first == key) {
                    return iter;
                }
                node_ref = std::ref(next);
            }
        }
        Iter begin = &node_ref.get() == &map._sentinel
            ? map.begin() : Iter {node_ref};
        auto end = map.end();
        auto result = std::find_if_not(begin, end, [&] (auto &value) {
            return value.first < key;
        });
        return result != end && result->first == key
            ? result : end;
    }

    template <typename This>
    static auto &_at(This &&map, const K &key) {
        auto iter = _find(std::forward<This>(map), key);
        if (iter == map.end()) {
            throw std::out_of_range{"Not found"};
        }
        return iter->second;
    }

    template <typename NodeRefIter, typename V>
    Iterator _insert_before(Iterator iter, std::size_t index,
                            NodeRefIter levels,
                            V &&value) {
        std::size_t height = _height_generator(_random);

        auto unique_node = std::make_unique<_DereferenceableNode>(
            std::forward<V>(value), height);
        auto node = unique_node.release();
        assert(node);
        iter.node().insert_before(*node, index, _sentinel, _size, levels);
        ++_size;
        return *node;
    }

    template <typename V>
    auto _insert_at_end(V &&value) {
        return _insert_before(end(), _size,
                              SentinelIter {_sentinel},
                              std::forward<V>(value));
    }

    template <typename Key>
    M &_subscript(Key &&key) {
        static_assert(std::is_default_constructible<M>::value,
                      "Mapped type must be default constructible");
        return _lower_bound(key).match([] (auto iter) {
            return iter;
        }, [&, this] (auto iter, auto index, auto levels) {
            return this->_insert_before(
                iter, index, levels,
                std::make_pair(std::forward<Key>(key), M{}));
        })->second;
    }

    template <class V>
    std::pair<Iterator, bool> _insert(V &&value) {
        return _lower_bound(value.first).match([] (auto iter) {
            return std::make_pair(iter, false);
        }, [&, this] (auto iter, auto index, auto levels) {
            return std::make_pair(
                this->_insert_before(iter, index, levels, std::forward<V>(value)),
                true);
        });
    }

    template <typename This>
    static auto _index(This &&map, std::size_t i) {
        if (i >= map.size()) return map.end();
        auto iter = map.begin();
        std::advance(iter, i);
        return iter;
    }

public:
    Map() :
        _sentinel{}, _size{},
        _random{std::random_device {}()}, _height_generator{} {
    }

    Map(const Map &that) : Map{} {
        for (auto &value : that) {
            _insert_at_end(value);
        }
    }

    Map(std::initializer_list<ValueType> pairs) : Map{} {
        insert(pairs.begin(), pairs.end());
    }

    Map &operator=(const Map &that) {
        if (this != &that) {
            clear();
            for (auto &value : that) {
                _insert_at_end(value);
            }
        }
        return *this;
    }

    ~Map() {
        clear();
    }

    constexpr std::size_t size() const {
        return _size;
    }

    constexpr bool empty() const {
        return size() == 0;
    }

    Iterator begin() {
        return _begin(*this);
    }

    Iterator end() {
        return _end(*this);
    }

    ConstIterator begin() const {
        return _begin(*this);
    }

    ConstIterator end() const {
        return _end(*this);
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
        return _subscript(key);
    }

    M &operator[](K &&key) {
        return _subscript(std::move(key));
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
        auto &node = iter.node();
        if (node.height() == _sentinel.height()) {
            _sentinel.shrink();
        }
        delete &node;
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
        while (!empty()) {
            erase(begin());
        }
    }

    Iterator index(std::size_t i) {
        return _index(*this, i);
    }

    ConstIterator index(std::size_t i) const {
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
