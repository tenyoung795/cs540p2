#include <cstddef>

#include <functional>
#include <random>

#include "Map.hpp"

namespace std {
template class reference_wrapper<cs540::internal::Node>;
template class geometric_distribution<size_t>;
}

namespace cs540 {
namespace internal {
void Link::insert_before(Node &prev, std::size_t i) {
    auto owner = _prev.get().links(i)._next;
    _prev.get().links(i)._next = std::ref(prev);
    prev.links(i)._prev = _prev;
    prev.links(i)._next = owner;
    _prev = std::ref(prev);
}

void Link::disconnect(std::size_t i) {
    auto owner = _prev.get().links(i)._next;
    _prev.get().links(i)._next = _next;
    _next.get().links(i)._prev = _prev;
    _prev = _next = owner;
}

Node::Node(std::size_t height) : Node{} {
    _links.reserve(height);
    for (std::size_t i = 0; i < height; ++i) {
        _links.emplace_back(*this);
    }
}

Node::~Node() {
    for (std::size_t i = 0; i < height(); ++i) {
        _links[i].disconnect(i);
    }
    _prev.get()._next = _next;
    _next.get()._prev = _prev;
}

void Node::grow(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        _links.emplace_back(*this);
    }
}

void Node::shrink() noexcept {
    for (auto i = height(); i > 0; --i) {
        auto &back = _links.back();
        if (&back.next() != this) {
            break;
        }
        _links.pop_back();
    }
}
} // namespace internal
} // namespace cs540
