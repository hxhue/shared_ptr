#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

// Before implementing this, I've read
// https://github.com/DanielLiamAnderson/atomic_shared_ptr. It's quite
// enlightening and helps me understand the internals of a shared pointer.

namespace lockfree {

// We don't have to implement a unique_ptr.
using ::std::unique_ptr;

namespace detail {
struct control_block {
  ::std::atomic<int> use_count;  // Strong count.
  ::std::atomic<int> weak_count; // Weak count + !!(strong count).

  control_block() : use_count(1), weak_count(1) {}

  control_block(int use, int weak) : use_count(use), weak_count(weak) {}

  virtual void destroy() = 0; // Called when use_count decrements to 0.

  // virtual void *getaddr() = 0;

  virtual ~control_block() = default;

  void increment_use_count() {
    if (0 == use_count.fetch_add(1, ::std::memory_order_relaxed)) {
      weak_count.fetch_add(1, ::std::memory_order_relaxed);
    }
  }

  void decrement_use_count() {
    int old_use_count = use_count.fetch_sub(1, ::std::memory_order_relaxed);
    assert(old_use_count > 0);
    if (old_use_count == 1) {
      destroy();
      int old_weak_count = weak_count.fetch_sub(1, ::std::memory_order_relaxed);
      assert(old_weak_count > 0);
      if (old_weak_count == 1) {
        delete this;
      }
    }
  }
};

struct monostate {};

template <typename T> struct DefaultDeleter {
  void operator()(void *ptr) const { delete static_cast<T *>(ptr); }
};

template <typename T> struct control_block_with_ptr : control_block {
  explicit control_block_with_ptr(T *ptr)
      : control_block_with_ptr(ptr, DefaultDeleter<T>()) {}

  template <typename Deleter>
  control_block_with_ptr(T *ptr, Deleter deleter)
      : ptr_(ptr), deleter_(std::move(deleter)) {}

  // void *getaddr() override { return static_cast<void *>(getptr()); }

  void destroy() override {
    deleter_(ptr_);
    ptr_ = nullptr;
  }

private:
  T *ptr_;
  ::std::function<void(T *)> deleter_;

  T *getptr() { return ptr_; }
};

// TODO: control_block_with_inplace_obj is incomplete.
// I think only control_block_with_ptr can have a deleter.
template <typename T> struct control_block_with_inplace_obj : control_block {
  explicit control_block_with_inplace_obj(T obj)
      : control_block(/* is this correct? */), obj_(std::move(obj)) {}

  // void *getaddr() override { return static_cast<void *>(getptr()); }

  void destroy() override {
    obj_.~T();
    empty_ = {};
  }

private:
  union {
    T obj_;
    monostate empty_;
  };

  T *getptr() { return &obj_; }
};

// template <typename T, typename Y>
// concept valid_ptr_conversion =
//     ::std::is_base_of_v<T, Y> || ::std::is_same_v<T, Y>;

template <typename T, typename Y>
concept valid_ptr_conversion = ::std::is_base_of_v<T, Y>;

} // namespace detail

template <typename T> struct shared_ptr {
private:
public:
  template <typename Y> friend class shared_ptr;

  // Constructors from
  // https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr.
  constexpr shared_ptr() noexcept : ptr_(nullptr), ctrl_(nullptr) {}

  constexpr shared_ptr(std::nullptr_t) noexcept
      : ptr_(nullptr), ctrl_(nullptr) {}

  template <typename Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  explicit shared_ptr(Y *ptr) : shared_ptr(ptr, detail::DefaultDeleter<T>{}) {}

  template <class Y, class Deleter>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(Y *ptr, Deleter d) {
    if (!ptr) {
      clear();
    } else {
      if (!(ptr_ = dynamic_cast<T *>(ptr))) {
        throw ::std::bad_cast{};
      }
      ctrl_ = new detail::control_block_with_ptr<T>(ptr_, std::move(d));
    }
  }

  template <class Deleter>
  shared_ptr(std::nullptr_t ptr, Deleter d)
      : shared_ptr(static_cast<T *>(nullptr), std::move(d)) {}

  // TODO
  template <class Y, class Deleter, class Alloc>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(Y *ptr, Deleter d, Alloc alloc);

  template <class Deleter, class Alloc>
  shared_ptr(std::nullptr_t ptr, Deleter d, Alloc alloc)
      : shared_ptr(static_cast<T *>(nullptr), std::move(d), std::move(alloc)) {}

  // Aliasing constructors
  template <class Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(const shared_ptr<Y> &r, T *ptr) noexcept : shared_ptr(r) {
    ptr_ = ptr;
  }

  template <class Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(shared_ptr<Y> &&r, T *ptr) noexcept : shared_ptr(r) {
    ptr_ = ptr;
  }

  shared_ptr(const shared_ptr &r) noexcept {
    if (!r.ctrl_) {
      clear();
    } else {
      r.ctrl_->increment_use_count();
      ctrl_ = r.ctrl_;
      ptr_ = r.ptr_;
    }
  }

  template <class Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(const shared_ptr<Y> &r) noexcept {
    if (!r.ctrl_) {
      clear();
    } else {
      auto t = static_cast<T *>(r.ptr_);
      r.ctrl_->increment_use_count();
      ptr_ = t;
      ctrl_ = r.ctrl_;
    }
  }

  shared_ptr(shared_ptr &&r) noexcept {
    clear();
    ::std::swap(ctrl_, r.ctrl_);
    ::std::swap(ptr_, r.ptr_);
  }

  template <class Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(shared_ptr<Y> &&r) noexcept {
    if (!r.ctrl_) {
      clear();
    } else {
      auto t = static_cast<T *>(r.ptr_);
      ptr_ = ::std::exchange(r.ptr_, nullptr);
      ctrl_ = ::std::exchange(r.ctrl_, nullptr);
    }
  }

  // TODO: weak_ptr
  // template <class Y> explicit shared_ptr(const std::weak_ptr<Y> &r);

  template <class Y, class Deleter>
    requires(detail::valid_ptr_conversion<T, Y>)
  shared_ptr(unique_ptr<Y, Deleter> &&r) {
    auto y = r.release();
    try {
      new (this) shared_ptr(y, r.get_deleter());
    } catch (...) {
      r.reset(y);
      throw;
    }
  }

  ~shared_ptr() { reset(); }

  // TODO: full list of operator= overloads
  shared_ptr &operator=(const shared_ptr &r) noexcept {
    shared_ptr temp{r};
    temp.swap(*this);
    return *this;
  }

  shared_ptr &operator=(shared_ptr &&r) noexcept {
    shared_ptr temp{r};
    temp.swap(*this);
    return *this;
  }

  void reset() {
    if (ctrl_) {
      ctrl_->decrement_use_count();
    }
    clear();
  }

  template <class Y>
    requires(detail::valid_ptr_conversion<T, Y>)
  void reset(Y *ptr) {
    shared_ptr temp{ptr};
    temp.swap(*this);
  }

  template <class Y, class Deleter>
    requires(detail::valid_ptr_conversion<T, Y>)
  void reset(Y *ptr, Deleter d) {
    shared_ptr temp{ptr, std::move(d)};
    temp.swap(*this);
  }

  // TODO
  template <class Y, class Deleter, class Alloc>
    requires(detail::valid_ptr_conversion<T, Y>)
  void reset(Y *ptr, Deleter d, Alloc alloc);

  void swap(shared_ptr &r) noexcept {
    ::std::swap(ptr_, r.ptr_);
    ::std::swap(ctrl_, r.ctrl_);
  }

  T *get() const noexcept { return ptr_; }

  T &operator*() const noexcept { return *ptr_; }

  T *operator->() const noexcept { return ptr_; }

  // TODO: element_type& operator[]( std::ptrdiff_t idx ) const;

  long use_count() const noexcept {
    return ctrl_ ? ctrl_->use_count.load() : 0;
  }

  explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
  T *ptr_;
  detail::control_block *ctrl_;

  void clear() {
    ptr_ = nullptr;
    ctrl_ = nullptr;
  }
};

// TODO: For now, use an inefficient implementation.
template <class T, class... Args> shared_ptr<T> make_shared(Args &&...args) {
  return shared_ptr<T>(new T(std::forward<Args...>(args...)));
}

} // namespace lockfree

template <class T, class U>
std::strong_ordering operator<=>(const lockfree::shared_ptr<T> &lhs,
                                 const lockfree::shared_ptr<U> &rhs) noexcept {
  auto lhs_ = static_cast<void *>(lhs.get());
  auto rhs_ = static_cast<void *>(rhs.get());
  return reinterpret_cast<uintptr_t>(lhs_) <=>
         reinterpret_cast<uintptr_t>(rhs_);
}

template <class T>
std::strong_ordering operator<=>(const lockfree::shared_ptr<T> &lhs,
                                 std::nullptr_t) noexcept {
  auto lhs_ = static_cast<void *>(lhs.get());
  return reinterpret_cast<uintptr_t>(lhs_) <=> 0;
}

// TODO: hash support