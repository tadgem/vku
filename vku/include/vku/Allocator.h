#pragma once
#include "Alias.h"
#include "Allocator.h"
#include "STLAlias.h"
#include <cstdlib>

namespace vku {
class IAllocator;

template <class T> struct STLAllocator {
  typedef T value_type;
  IAllocator *_allocator;

  STLAllocator(IAllocator *alloc = nullptr)
      : _allocator(alloc) {
  } // default ctor not required by C++ Standard Library

  // A converting copy constructor:
  template <class U>
  STLAllocator(const STLAllocator<U> &o) : _allocator(o._allocator) {}
  template <class U> bool operator==(const STLAllocator<U> &) const {
    return true;
  }
  template <class U> bool operator!=(const STLAllocator<U> &) const {
    return false;
  }

  static T *fallback_allocate(const size_t n) {
    // log error or warning
    return static_cast<T *>(std::malloc(sizeof(T) * n));
  }

  static void fallback_deallocate(T *const p, size_t) { std::free(p); }

  T *allocate(const size_t n) const {
    if (_allocator == nullptr) {
      return fallback_allocate(n);
    }
    return static_cast<T *>(_allocator->allocate(sizeof(T) * n));
  }
  void deallocate(T *const p, size_t n) const {
    if (_allocator == nullptr) {
      fallback_deallocate(p, n);
    } else {
      _allocator->deallocate((void *)p);
    }
  }
};

class IAllocator {
public:
  virtual void *allocate(size_t size) = 0;
  virtual void deallocate(void *addr) = 0;

  virtual ~IAllocator() {}

  template <typename T> operator STLAllocator<T>() noexcept {
    return STLAllocator<T>(this);
  }
};

class MallocAllocator : public IAllocator {
public:
  void *allocate(size_t size) override { return VKU_MEMORY_NS::malloc(size); }

  void deallocate(void *addr) override { VKU_MEMORY_NS::free(addr); }
};
} // namespace vku