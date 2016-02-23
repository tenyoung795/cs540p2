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
#include <tuple>
#include <type_traits>
#include <utility>

namespace cs540 {
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

class Link {
    struct _MovingLink {
        Link &&link;
        _MovingLink(Link &&link, Link &sentinel) : link{std::move(link)} {
            link._prev.get()._next = link._next.get()._prev = std::ref(sentinel);
        }
        ~_MovingLink() {
            link._prev = link._next = std::ref(link);
        }
    };

    std::reference_wrapper<Link> _prev, _next;

    Link(_MovingLink &&link) : _prev{link.link._prev}, _next{link.link._next} {}

protected:
    Link() : _prev{*this}, _next{*this} {}
    Link(Link &&that) : Link{_MovingLink{std::move(that), *this}} {}

    Link &operator=(Link &&that) {
        _MovingLink moving_link{std::move(that), *this};
        _prev = moving_link.link._prev;
        _next = moving_link.link._next;
        return *this;
    }

    void insert_before(Link &prev) {
        _prev.get()._next = std::ref(prev);
        prev._prev = _prev;
        prev._next = std::ref(*this);
        _prev = std::ref(prev);
    }

    void disconnect() {
        _prev.get()._next = _next;
        _next.get()._prev = _prev;
        _prev = _next = std::ref(*this);
    }

    friend class Node;

public:
    Link &prev() const {
        return _prev;
    }

    Link &next() const {
        return _next;
    }

    bool empty() const {
        return &next() == this;
    }
}; // class Link

class Node;
using NodeWithLink = std::pair<Node, Link>;

class Node {
    struct _MovingNode {
        Node &&node;
        _MovingNode(Node &&node, std::reference_wrapper<Node> sentinel) : node{std::move(node)} {
            node._prev.get()._next = node._next.get()._prev = sentinel;
        }
        ~_MovingNode() {
            node._prev = node._next = std::ref(node);
        }
    };

    DefaultOnMove<std::size_t> _height;
    std::reference_wrapper<Node> _prev, _next;

    Node(_MovingNode &&node) :
        _height{std::move(node.node._height)},
        _prev{node.node._prev}, _next{node.node._next} {
        for (std::size_t i = 0; i < _height; ++i) {
            new (&links(i)) Link{std::move(node.node.links(i))};
        }
    }

    Link &links(std::size_t i) {
        return reinterpret_cast<Link *>(
            reinterpret_cast<char *>(this)
            + offsetof(NodeWithLink, second))[i];
    }

public:
    Node(std::size_t max_height) : _height{max_height}, _prev{*this}, _next{*this} {
        for (std::size_t i = 0; i < _height; ++i) {
            new (&links(i)) Link;
        }
    }

    Node(Node &&that) : Node{_MovingNode {std::move(that), *this}} {}

    Node &operator=(Node &&that) {
        _MovingNode moving_node{std::move(that), *this};
        _height = std::move(moving_node.node._height);
        _prev = moving_node.node._prev;
        _next = moving_node.node._next;
        for (std::size_t i = 0; i < _height; ++i) {
            links(i) = std::move(moving_node.node.links(i));
        }
        return *this;
    }

    std::size_t &height() {
        return _height;
    }

    const std::size_t &height() const {
        return _height;
    }

    Node &prev() const {
        return _prev;
    }

    Node &next() const {
        return _next;
    }

    const Link &links(std::size_t i) const {
        return reinterpret_cast<const Link *>(
            reinterpret_cast<const char *>(this)
            + offsetof(NodeWithLink, second))[i];
    }

    static Node &from_link(Link &link, std::size_t i) {
        return *reinterpret_cast<Node *>(
            reinterpret_cast<char *>(&link - i)
            - offsetof(NodeWithLink, second));
    }

    static const Node &from_link(std::reference_wrapper<const Link> link, std::size_t i) {
        return *reinterpret_cast<const Node *>(
            reinterpret_cast<const char *>(&link.get() - i)
            - offsetof(NodeWithLink, second));
    }

    template <typename LinkRefIter>
    void insert_before(Node &prev, LinkRefIter link_iter) {
        static_assert(
            std::is_convertible<
                typename std::iterator_traits<LinkRefIter>::reference,
                Link &>::value,
            "LinkRefIter's reference must be convertible to Link &");
        _prev.get()._next = std::ref(prev);
        prev._prev = _prev;
        prev._next = std::ref(*this);
        _prev = std::ref(prev);
        for (std::size_t i = 0; i < prev._height; ++i) {
            static_cast<Link &>(*link_iter).insert_before(prev.links(i));
            ++link_iter;
        }
    }

    void disconnect() {
        for (std::size_t i = _height; i > 0; --i) {
            links(i - 1).disconnect();
        }
        _prev.get()._next = _next;
        _next.get()._prev = _prev;
        _prev = _next = std::ref(*this);
    }
}; // class Node

template <bool Const, typename T>
using ConstOrMutT = std::conditional_t<Const, const T, std::remove_const_t<T>>;

template <typename>
struct MapArrayImpl;

template <std::size_t... I>
struct MapArrayImpl<std::index_sequence<I...>> {
    template <typename F, typename T, std::size_t N>
    static std::array<std::result_of_t<F(T &)>, N> call(
        const F &f, std::array<T, N> &xs) {
        return {f(std::get<I>(xs))...};
    }
};

template <typename F, typename T, std::size_t N>
auto map_array(const F &f, std::array<T, N> &xs) {
    return MapArrayImpl<std::make_index_sequence<N>>::call(f, xs);
}

class FreeNode {
    struct _Deleter {
        void operator()(FreeNode *node) {
            node->~FreeNode();
            delete[] reinterpret_cast<char *>(node);
        }
    };

public:
    using UniquePtr = std::unique_ptr<FreeNode, _Deleter>;

private:
    FreeNode() noexcept = default;

    ~FreeNode() {
        while (next) {
            next = std::move(next->next);
        }
    }

public:
    UniquePtr next;

    static UniquePtr make(std::size_t size) {
        assert(size >= sizeof(FreeNode));
        return UniquePtr {new(new char[size]) FreeNode};
    }

    template <typename T, typename... Args>
    static T &use(UniquePtr &head, Args &&...args) {
        assert(head);
        auto next = std::move(head->next);
        auto space = std::move(head);
        try {
            auto &result = *new(&*space) T {std::forward<Args>(args)...};
            space.release();
            head = std::move(next);
            return result;
        } catch (...) {
            head = std::move(space);
            head->next = std::move(next);
            throw;
        }
    }

    template <typename T>
    static UniquePtr release(T &space) {
        space.~T();
        return UniquePtr {new(&space) FreeNode};
    }
}; // class FreeNode
} // anonymous namespace

template <typename K, typename M>
class Map {
public:
    using ValueType = std::pair<const K, M>;

private:
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
            return *reinterpret_cast<ConstOrMutT<Const, ValueType> *>(
                reinterpret_cast<ConstOrMutT<Const, char> *>(_node)
                - offsetof(_DereferenceableNode, node));
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
    static constexpr std::size_t _MAX_HEIGHT = 31;
    using _LinkRefs = std::array<std::reference_wrapper<Link>, _MAX_HEIGHT>;

    struct _DereferenceableNode {
        ValueType value;
        Node node;

        template <typename V>
        _DereferenceableNode(V &&value, std::size_t height) :
            value{std::forward<V>(value)}, node{height} {}
    };
    using _DereferenceableNodeWithLink = std::pair<_DereferenceableNode, Link>;

    class _SearchResult {
        Iterator _iter;
        union {
            _LinkRefs _link_refs;
        };
        bool _found;

    public:
        explicit _SearchResult(Iterator iter) : _iter{iter}, _found{true} {}

        explicit _SearchResult(Iterator iter, _LinkRefs link_refs)
            : _iter{iter}, _link_refs{link_refs}, _found{false} {}

        template <typename F, typename G>
        auto match(F &&found, G &&missing) const {
            return _found
                ? std::forward<F>(found)(_iter)
                : std::forward<G>(missing)(_iter, _link_refs.begin());
        }
    };

    Node _sentinel;
    union {
        // _sentinel will assume it constructs these links.
        std::array<Link, _MAX_HEIGHT> _links;
    };
    DefaultOnMove<std::size_t> _size;
    std::default_random_engine _random;
    std::bernoulli_distribution _flip_coin;
    std::array<FreeNode::UniquePtr, _MAX_HEIGHT + 1> _free_nodes;

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
        _LinkRefs link_refs = map_array([] (auto &x) {
            return std::ref(x);
        }, _links);

        if (empty() || Iterator {_sentinel.prev()}->first < key) {
            return _SearchResult {end(), link_refs};
        }

        for (auto i = _sentinel.height(); i > 0; --i) {
            auto &link_ref = link_refs[i - 1];
            if (i < _sentinel.height()) {
                link_ref = std::ref((&link_refs[i].get())[-1]);
            }
            while (true) {
                Iterator iter{Node::from_link(link_ref.get().next(), i - 1)};
                if (&link_ref.get().next() == &_links[i - 1] || key < iter->first) break;
                if (iter->first == key) {
                    return _SearchResult {iter};
                }
                link_ref = std::ref(link_ref.get().next());
            }
        }

        Iterator begin = _sentinel.height() == 0 || &link_refs[0].get() == &_links[0]
            ? this->begin()
            : Iterator {Node::from_link(link_refs[0], 0)};
        auto end = this->end();
        auto result = std::find_if_not(begin, end, [&] (auto &value) {
            return value.first < key;
        });
        if (result != end && result->first == key) {
            return _SearchResult {result};
        }
        return _SearchResult {result, link_refs};
    }

    template <typename This>
    static auto _find(This &&map, const K &key) {
        using Iter = decltype(map.begin());

        if (map.empty() || _iter(map._sentinel.prev())->first < key) {
            return map.end();
        }

        Iter begin = map.begin();
        if (map._sentinel.height() > 0) {
            auto link_ref = std::ref(map._links[map._sentinel.height() - 1]);
            for (auto i = map._sentinel.height(); i > 0; --i) {
                if (i < map._sentinel.height()) {
                    link_ref = std::ref((&link_ref.get())[-1]);
                }
                while (true) {
                    Iter iter{Node::from_link(link_ref.get().next(), i - 1)};
                    if (&link_ref.get().next() == &map._links[i - 1] || key < iter->first) break;
                    if (iter->first == key) {
                        return iter;
                    }
                    link_ref = decltype(link_ref) {link_ref.get().next()};
                }
            }
            if (&link_ref.get() != &map._links[0]) {
                begin = Iter {Node::from_link(link_ref, 0)};
            }
        }
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

    auto &_find_free_node(std::size_t height) {
        for (auto i = height; i <= _MAX_HEIGHT; ++i) {
            if (_free_nodes[i]) return _free_nodes[i];
        }
        auto free_node = FreeNode::make(
            sizeof(_DereferenceableNode)
            + offsetof(_DereferenceableNodeWithLink, second)
            + height * sizeof(Link));
        free_node->next = std::move(_free_nodes[height]);
        _free_nodes[height] = std::move(free_node);
        return _free_nodes[height];
    }

    template <typename LinkRefIter, typename V>
    Iterator _insert_before(Iterator iter, LinkRefIter link_iter, V &&value) {
        std::size_t height = 0;
        for (; height < _MAX_HEIGHT && _flip_coin(_random); ++height);

        auto &node = FreeNode::use<_DereferenceableNode>(
            _find_free_node(height), std::forward<V>(value), height);
        iter.node().insert_before(node.node, link_iter);
        ++_size;
        _sentinel.height() = std::max(_sentinel.height(), height);
        return node.node;
    }

    template <typename V>
    auto _insert_at_end(V &&value) {
        return _insert_before(end(), _links.begin(), std::forward<V>(value));
    }

    template <typename Key>
    M &_subscript(Key &&key) {
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
        _sentinel{_MAX_HEIGHT}, _size{},
        _random{std::random_device {}()}, _flip_coin{},
        _free_nodes{} {
        _sentinel.height() = 0;
    }

    Map(const Map &that) : Map{} {
        for (auto &value : that) {
            _insert_at_end(value);
        }
    }

    Map(Map &&that) = default;

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

    Map &operator=(Map &&that) = default;

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
        auto height = node.height();
        node.disconnect();
        auto free_node = FreeNode::release(*reinterpret_cast<_DereferenceableNode *>(
            reinterpret_cast<char *>(&node)
            - offsetof(_DereferenceableNode, node)));
        free_node->next = std::move(_free_nodes[height]);
        _free_nodes[height] = std::move(free_node);
        if (height == _sentinel.height()) {
            for (; _sentinel.height() > 0 && _links[_sentinel.height() - 1].empty();
                --_sentinel.height());
        }
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
