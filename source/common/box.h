#ifndef BOX_H
#define BOX_H

#include <memory>

// https://www.foonathan.net/2022/05/recursive-variant-box/

template <typename T>
class box
{
    // Wrapper over unique_ptr.
    std::unique_ptr<T> _impl;

public:
    // Automatic construction from a `T`, not a `T*`.
    box(T &&obj) : _impl(new T(std::move(obj))) {}
    box(const T &obj) : _impl(new T(obj)) {}
    box() : _impl() {}

    // Copy constructor copies `T`.
    box(const box &other) : _impl(other._impl ? new T(*other._impl) : nullptr) {}
    box &operator=(const box &other)
    {
        if (other._impl) {
            if (_impl) {
                *_impl = *other._impl;
            } else {
                _impl = std::make_unique<T>(*other._impl);
            }
        } else {
            _impl = nullptr;
        }
        return *this;
    }

    // unique_ptr destroys `T` for us.
    ~box() = default;

    // Access propagates constness.
    T &operator*() { return *_impl; }
    const T &operator*() const { return *_impl; }

    T *operator->() { return _impl.get(); }
    const T *operator->() const { return _impl.get(); }
};

#endif //BOX_H
