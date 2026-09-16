#pragma once
#include "vku/Allocator.h"
#include "vku/STLAlias.h"

#include VKU_IOSTREAM_ALIAS
#include VKU_UNORDERED_MAP_ALIAS
#include VKU_VECTOR_ALIAS
#include VKU_SPAN_ALIAS
#include VKU_FUNCTIONAL_ALIAS
#include VKU_ARRAY_ALIAS
#include VKU_OPTIONAL_ALIAS
#include VKU_MEMORY_ALIAS
#include VKU_STRING_ALIAS
#include VKU_STRING_STREAM_ALIAS

namespace vku {
using String =
    VKU_STRING_NS::basic_string<char, VKU_STRING_NS::char_traits<char>,
                                STLAllocator<char>>;

using StringStream = VKU_STRING_STREAM_NS::basic_stringstream<
    char, VKU_STRING_STREAM_NS::char_traits<char>, STLAllocator<char>>;

using IStringStream = VKU_STRING_STREAM_NS::basic_istringstream<
    char, VKU_STRING_STREAM_NS::char_traits<char>, STLAllocator<char>>;

template <typename _Ty>
using Vector = VKU_VECTOR_NS::vector<_Ty, STLAllocator<_Ty>>;

template <typename _Ty> using StaticVector = VKU_VECTOR_NS::vector<_Ty>;

template <typename _Ty, size_t _Size>
using Array = VKU_ARRAY_NS::array<_Ty, _Size>;

template <typename _Ty> using Span = VKU_SPAN_NS::span<_Ty>;

template <typename _Key, typename _Value>
using HashMap = VKU_UNORDERED_MAP_NS::unordered_map<
    _Key, _Value, VKU_UNORDERED_MAP_NS::hash<_Key>,
    VKU_UNORDERED_MAP_NS::equal_to<_Key>,
    STLAllocator<VKU_UNORDERED_MAP_NS::pair<const _Key, _Value>>>;

template <typename _Ty> using Optional = VKU_OPTIONAL_NS::optional<_Ty>;

template <typename _Ty> using Unique = VKU_MEMORY_NS::unique_ptr<_Ty>;

template <typename _Ty> using RefCntPtr = VKU_MEMORY_NS::shared_ptr<_Ty>;
} // namespace vku
