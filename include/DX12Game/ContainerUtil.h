#pragma once

#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#ifndef GARRAY
	#define GARRAY GArray<T, N>
#endif

#ifndef GARRAY_SIZE_TYPE
	#define GARRAY_SIZE_TYPE typename GARRAY::SizeType
#endif

#ifndef GARRAY_TEMPLATE
	#define GARRAY_TEMPLATE template <typename T, size_t N>
#endif

#ifndef GARRAY_ITERATOR
	#define GARRAY_ITERATOR typename GARRAY::Iterator
#endif

#ifndef GARRAY_CONST_ITERATOR
	#define GARRAY_CONST_ITERATOR typename GARRAY::ConstIterator
#endif

#ifndef GARRAY_REVERSE_ITERATOR
	#define GARRAY_REVERSE_ITERATOR typename GARRAY::ReverseIterator
#endif

#ifndef GARRAY_CONST_REVERSE_ITERATOR
	#define GARRAY_CONST_REVERSE_ITERATOR typename GARRAY::ConstReverseIterator
#endif

template <typename T, size_t N>
class GArray {
public:
	using SizeType = size_t;
	using Array = typename std::array<T, N>;
	using Iterator = typename Array::iterator;
	using ConstIterator = typename Array::const_iterator;
	using ReverseIterator = std::reverse_iterator<Iterator>;
	using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
	using Reference = T&;
	using ConstReference = const T&;

public:
	GArray() = default;
	virtual ~GArray() = default;

public:
	///
	// Element access
	///
	Reference at(SizeType inPos);
	constexpr ConstReference at(SizeType inPos) const;

	Reference operator[](SizeType inPos) noexcept;
	constexpr ConstReference operator[](SizeType inPos) const;

	Reference front();
	constexpr ConstReference front() const;

	Reference back();
	constexpr ConstReference back() const;

	T* data() noexcept;
	const T* data() const noexcept;
	/// Element access

	///
	//  Iterators
	///
	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;

	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;

	ReverseIterator rbegin() noexcept;
	ConstReverseIterator rbegin() const noexcept;
	ConstReverseIterator crbegin() const noexcept;

	ReverseIterator rend() noexcept;
	ConstReverseIterator rend() const noexcept;
	ConstReverseIterator crend() const noexcept;
	///  Iterators

	///
	// Capacity
	///
	constexpr bool empty() const noexcept;
	constexpr SizeType size() const noexcept;
	constexpr SizeType max_size() const noexcept;
	/// Capacity

	///
	// Operations
	///
	void fill(const T& inValue);

	void swap(Array& inOther) noexcept;
	/// Operations

private:
	Array mArray;
};

#ifndef GVECTOR
	#define GVECTOR GVector<T, Allocator>
#endif

#ifndef GVECTOR_ALLOCATOR_TYPE
	#define GVECTOR_ALLOCATOR_TYPE typename GVECTOR::AllocatorType
#endif

#ifndef GVECTOR_SIZE_TYPE
	#define GVECTOR_SIZE_TYPE typename GVECTOR::SizeType
#endif

#ifndef GVECTOR_TEMPLATE
	#define GVECTOR_TEMPLATE template <typename T, typename Allocator>
#endif

#ifndef GVECTOR_ITERATOR 
	#define GVECTOR_ITERATOR typename GVECTOR::Iterator
#endif

#ifndef GVECTOR_CONST_ITERATOR 
	#define GVECTOR_CONST_ITERATOR typename GVECTOR::ConstIterator
#endif

#ifndef GVECTOR_REVERSE_ITERATOR 
	#define GVECTOR_REVERSE_ITERATOR typename GVECTOR::ReverseIterator
#endif

#ifndef GVECTOR_CONST_REVERSE_ITERATOR 
	#define GVECTOR_CONST_REVERSE_ITERATOR typename GVECTOR::ConstReverseIterator
#endif

#ifndef GVECTOR_REFERENCE
	#define GVECTOR_REFERENCE typename GVECTOR::Reference
#endif

#ifndef GVECTOR_CONST_REFERENCE
	#define GVECTOR_CONST_REFERENCE typename GVECTOR::ConstReference
#endif

template <typename T, typename Allocator = std::allocator<T>>
class GVector {
public:
	using SizeType = size_t;
	using AllocatorType = Allocator;
	using Vector = typename std::vector<T, Allocator>;
	using Iterator = typename Vector::iterator;
	using ConstIterator = typename Vector::const_iterator;
	using ReverseIterator = std::reverse_iterator<Iterator>;
	using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
	using Reference = T&;
	using ConstReference = const T&;
	using RightValue = T&&;
	using InitializerList = std::initializer_list<T>;

public:
	GVector() = default;
	GVector(SizeType inSize);
	GVector(InitializerList inList);
	GVector& operator=(InitializerList inList);
	virtual ~GVector() = default;

public:
	void assgin(SizeType inCount, ConstReference inValue);
	template <typename InputIt> void assign(InputIt inFirst, InputIt inLast);
	void assgin(InitializerList inIList);

	AllocatorType get_allocator() const noexcept;

	///
	// Element access
	///
	Reference at(SizeType inPos);
	constexpr ConstReference at(SizeType inPos) const;

	Reference operator[](SizeType inIdx);
	constexpr ConstReference operator[](SizeType inIdx) const;

	Reference front();
	constexpr ConstReference front() const;

	Reference back();
	constexpr ConstReference back() const;

	T* data() noexcept;
	const T* data() const noexcept;
	/// Element access

	///
	// Iterators
	///
	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;

	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;

	ReverseIterator rbegin() noexcept;
	ConstReverseIterator rbegin() const noexcept;
	ConstReverseIterator crbegin() const noexcept;

	ReverseIterator rend() noexcept;
	ConstReverseIterator rend() const noexcept;
	ConstReverseIterator crend() const noexcept;
	/// Iterators

	///
	// Capacity
	///
	bool empty() const noexcept;
	SizeType size() const noexcept;
	SizeType max_size() const noexcept;

	void reserve(SizeType inNewCapacity);

	SizeType capacity() const noexcept;

	void shrink_to_fit();
	/// Capacity

	///
	// Modifiers
	///
	void clear() noexcept;

	Iterator insert(ConstIterator inPos, ConstReference inValue);
	Iterator insert(ConstIterator inPos, RightValue inValue);
	Iterator insert(ConstIterator inPos, const SizeType inCount, ConstReference inVal);
	template <class InputIt> Iterator insert(ConstIterator inPos, InputIt inFirst, InputIt inLast);
	Iterator insert(ConstIterator inPos, InitializerList inList);

	template <typename... Args>	Iterator emplace(ConstIterator inPos, Args&&... inArgs);

	Iterator erase(ConstIterator inPos);
	Iterator erase(ConstIterator inFirst, ConstIterator inLast);

	void push_back(ConstReference inValue);
	void push_back(RightValue inValue);

	template <typename... Args>	decltype(auto) emplace_back(Args&&... inArgs);

	void pop_back();

	void resize(SizeType inCount);
	void resize(SizeType inCount, ConstReference inValue);

	void swap(GVector& inOther);
	/// Modifiers

private:
	Vector mVector;
};

#ifndef GVECTOR_BOOL
	#define GVECTOR_BOOL GVector<bool, std::allocator<bool>> 
#endif

///
// GVector specialization for boolean
///
template <>
class GVector<bool, std::allocator<bool>> {
public:
	using SizeType = size_t;
	using AllocatorType = std::allocator<bool>;
	using Vector = typename std::vector<bool, AllocatorType>;
	using Iterator = typename Vector::iterator;
	using ConstIterator = typename Vector::const_iterator;
	using ReverseIterator = std::reverse_iterator<Iterator>;
	using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
	using Reference = bool&;
	using ConstReference = const bool&;
	using InitializerList = std::initializer_list<bool>;

public:
	GVector() = default;
	GVector(SizeType inSize);
	GVector(InitializerList inList);
	GVector& operator=(InitializerList inList);
	virtual ~GVector() = default;

public:
	void assgin(SizeType inCount, ConstReference inValue);
	template <typename InputIt> void assign(InputIt inFirst, InputIt inLast);
	void assgin(InitializerList inIList);

	AllocatorType get_allocator() const noexcept;

	///
	// Element access
	///
	Reference at(SizeType inPos);
	ConstReference at(SizeType inPos) const;

	Reference operator[](SizeType inIdx);
	ConstReference operator[](SizeType inIdx) const;

	Reference front();
	ConstReference front() const;

	Reference back();
	ConstReference back() const;
	/// Element access

	///
	// Iterators
	///
	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;

	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;

	ReverseIterator rbegin() noexcept;
	ConstReverseIterator rbegin() const noexcept;
	ConstReverseIterator crbegin() const noexcept;

	ReverseIterator rend() noexcept;
	ConstReverseIterator rend() const noexcept;
	ConstReverseIterator crend() const noexcept;
	/// Iterators

	///
	// Capacity
	///
	bool empty() const noexcept;
	SizeType size() const noexcept;
	SizeType max_size() const noexcept;

	void reserve(SizeType inNewCapacity);

	SizeType capacity() const noexcept;

	void shrink_to_fit();
	/// Capacity

	///
	// Modifiers
	///
	void clear() noexcept;

	Iterator insert(ConstIterator inPos, ConstReference inValue);
	//Iterator insert(ConstIterator inPos, RightValue inValue);
	Iterator insert(ConstIterator inPos, const SizeType inCount, ConstReference inVal);
	template <class InputIt> Iterator insert(ConstIterator inPos, InputIt inFirst, InputIt inLast);
	Iterator insert(ConstIterator inPos, InitializerList inList);

	template <typename... Args>	Iterator emplace(ConstIterator inPos, Args&&... inArgs);

	Iterator erase(ConstIterator inPos);
	Iterator erase(ConstIterator inFirst, ConstIterator inLast);

	void push_back(ConstReference inValue);
	//void push_back(RightValue inValue);

	template <typename... Args>	decltype(auto) emplace_back(Args&&... inArgs);

	void pop_back();

	void resize(SizeType inCount);
	void resize(SizeType inCount, ConstReference inValue);

	void swap(GVector& inOther);
	/// Modifiers

private:
	Vector mVector;
};

#ifndef UNORDEREDMAP
	#define UNORDEREDMAP GUnorderedMap<KeyType, ValType, Hasher, KeyEql, Allocator>
#endif

#ifndef UNORDERED_TEMPLATE
	#define UNORDERED_TEMPLATE	\
		template <typename KeyType, typename ValType, typename Hasher, typename KeyEql, typename Allocator>
#endif

#ifndef UNORDERED_ITERATOR
	#define UNORDERED_ITERATOR typename UNORDEREDMAP::Iterator
#endif

#ifndef UNORDERED_CONST_ITERATOR
	#define UNORDERED_CONST_ITERATOR typename UNORDEREDMAP::ConstIterator
#endif

#ifndef UNORDERED_LOCAL_ITERATOR
	#define UNORDERED_LOCAL_ITERATOR typename UNORDEREDMAP::LocalIterator
#endif

#ifndef UNORDERED_LOCAL_CONST_ITERATOR
	#define UNORDERED_LOCAL_CONST_ITERATOR typename UNORDEREDMAP::LocalConstIterator
#endif

template <typename KeyType,
		  typename ValType,
		  typename Hasher = std::hash<KeyType>,
		  typename KeyEql = std::equal_to<KeyType>,
		  typename Allocator = std::pmr::polymorphic_allocator<std::pair<const KeyType, ValType>>>
class GUnorderedMap {
public:
	using SizeType = size_t;
	using UnorderedMap = typename std::unordered_map<KeyType, ValType, Hasher, KeyEql, Allocator>;
	using Iterator = typename UnorderedMap::iterator;
	using ConstIterator = typename UnorderedMap::const_iterator;
	using LocalIterator = Iterator;
	using LocalConstIterator = ConstIterator;

public:
	GUnorderedMap() = default;
	virtual ~GUnorderedMap() = default;

public:
	GUnorderedMap& operator=(const GUnorderedMap& inOther);
	GUnorderedMap& operator=(GUnorderedMap&& inOther);

public:
	///
	// Iterators
	///
	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;

	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;
	/// Iterators

	///
	// Capacity
	///
	bool empty() const noexcept;
	SizeType size() const noexcept;
	SizeType max_size() const noexcept;
	/// Capacity

	///
	// Modifiers
	///
	void clear() noexcept;
	template <typename... Args> std::pair<Iterator, bool> emplace(Args&&... inArgs);
	template <typename... Args> Iterator emplace_hint(ConstIterator inHint, Args&&... inArgs);
	Iterator erase(ConstIterator inPos);
	Iterator erase(ConstIterator inFirst, ConstIterator inLast);
	SizeType erase(const KeyType& inKey);
	/// Modifiers

	///
	// Lookup
	///
	ValType& at(const KeyType& inKey);
	const ValType& at(const KeyType& inKey) const;

	ValType& operator[](const KeyType& inKey);
	ValType& operator[](KeyType&& inKey);
	SizeType count(const KeyType& inKey) const;

	Iterator find(const KeyType& inKey);
	ConstIterator find(const KeyType& inKey) const;

	std::pair<Iterator, Iterator> equal_range(const KeyType& inKey);
	std::pair<ConstIterator, ConstIterator> equal_range(const KeyType& inKey) const;
	/// Lookup

	///
	// Bucket interface
	///
	LocalIterator begin(SizeType inBucket);
	LocalConstIterator begin(SizeType inBucket) const;
	LocalConstIterator cbegin(SizeType inBucket) const;

	LocalIterator end(SizeType inBucket);
	LocalConstIterator end(SizeType inBucket) const;
	LocalConstIterator cend(SizeType inBucket) const;
	/// Bucket interface

private:
	UnorderedMap mUnorderedMap;
};

#include "DX12Game/ContainerUtil.inl"