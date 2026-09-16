#pragma once
#include "vku/Alias.h"
#include VKU_STDLIB_ALIAS

namespace vku
{
	class IAllocator;

	template <class T>
	struct STLAllocator
	{
		typedef T value_type;
		IAllocator& _allocator;

		STLAllocator(IAllocator& alloc) : _allocator(alloc) {} //default ctor not required by C++ Standard Library

		// A converting copy constructor:
		template<class U> STLAllocator(const STLAllocator<U>& o) : _allocator(o._allocator) {}
		template<class U> bool operator==(const STLAllocator<U>&) const
		{
			return true;
		}
		template<class U> bool operator!=(const STLAllocator<U>&) const
		{
			return false;
		}
		T* allocate(const size_t n) const
		{
			return static_cast<T*>(_allocator.allocate(sizeof(T) * n));
		}
		void deallocate(T* const p, size_t) const
		{
			_allocator.deallocate((void*)p);
		}
	};

	class IAllocator
	{
	public:
		virtual void* allocate(size_t size) = 0;
		virtual void  deallocate(void* addr) = 0;

		virtual ~IAllocator() {}

		template<typename T>
		operator STLAllocator<T>() noexcept
		{
			return STLAllocator<T>(*this);
		}
	};


	class MallocAllocator : public IAllocator
	{
	public:
		void* allocate(size_t size) override {
			return VKU_MEMORY_NS::malloc(size);
		}

		void deallocate(void* addr)
		{
			VKU_MEMORY_NS::free(addr);
		}
	};
}