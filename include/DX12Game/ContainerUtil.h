#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#ifndef GVECTOR
	#define GVECTOR GVector<T, Allocator>
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

template <typename T, typename Allocator = std::allocator<T>>
class GVector {
private:
	using SizeType = size_t;
	using Vector = typename std::vector<T, Allocator>;
	using Iterator = typename Vector::iterator;
	using ConstIterator = typename Vector::const_iterator;
	using ReverseIterator = std::reverse_iterator<Iterator>;
	using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
	GVector() = default;
	GVector(SizeType inSize);
	GVector(std::initializer_list<T> inList);
	virtual ~GVector() = default;

public:
	Vector& operator=(std::initializer_list<T> inList);

	T& at(SizeType inPos);
	const T& at(SizeType inPos) const;
	T& operator[](SizeType inIdx);
	const T& operator[](SizeType inIdx) const;
	T& front();
	const T& front() const;
	T& back();
	const T& back() const;
	T* data() noexcept;
	const T* data() const noexcept;

	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;
	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;
	ReverseIterator rbegin() noexcept;
	ConstReverseIterator crbegin() const noexcept;
	ReverseIterator rend() noexcept;
	ConstReverseIterator crend() const noexcept;

	bool empty() const noexcept;
	SizeType size() const noexcept;
	SizeType max_size() const noexcept;
	void reserve(SizeType inNewCapacity);
	SizeType capacity() const noexcept;
	void shrink_to_fit();

	void clear() noexcept;
	Iterator insert(ConstIterator inPos, const T& inValue);
	Iterator insert(ConstIterator inPos, T&& inValue);
	Iterator insert(ConstIterator inWhere, const SizeType inCount, const T& inVal);
	Iterator insert(ConstIterator inWhere, Iterator inFirst, Iterator inLast);
	Iterator insert(ConstIterator inWhere, std::initializer_list<T> inList);
	template <typename... Args>	Iterator emplace(ConstIterator inPos, Args&&... inArgs);
	Iterator erase(ConstIterator inPos);
	Iterator erase(ConstIterator inFirst, ConstIterator inLast);
	void push_back(const T& inValue);
	void push_back(T&& inValue);
	template <typename... Args>	decltype(auto) emplace_back(Args&&... inArgs);
	void pop_back();
	void resize(SizeType inCount);
	void swap(GVector& inOther);

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

	Iterator begin() noexcept;
	ConstIterator begin() const noexcept;
	ConstIterator cbegin() const noexcept;
	Iterator end() noexcept;
	ConstIterator end() const noexcept;
	ConstIterator cend() const noexcept;

	bool empty() const noexcept;
	SizeType size() const noexcept;
	SizeType max_size() const noexcept;

	void clear() noexcept;
	template <typename... Args> std::pair<Iterator, bool> emplace(Args&&... inArgs);
	template <typename... Args> Iterator emplace_hint(ConstIterator inHint, Args&&... inArgs);
	Iterator erase(ConstIterator inPos);
	Iterator erase(ConstIterator inFirst, ConstIterator inLast);

	ValType& at(const KeyType& inKey);
	const ValType& at(const KeyType& inKey) const;
	ValType& operator[](const KeyType& inKey);
	ValType& operator[](KeyType&& inKey);
	SizeType count(const KeyType& inKey) const;
	Iterator find(const KeyType& inKey);
	ConstIterator find(const KeyType& inKey) const;
	std::pair<Iterator, Iterator> equal_range(const KeyType& inKey);
	std::pair<ConstIterator, ConstIterator> equal_range(const KeyType& inKey) const;
	SizeType erase(const KeyType& inKey);

	LocalIterator begin(SizeType inBucket);
	LocalConstIterator begin(SizeType inBucket) const;
	LocalConstIterator cbegin(SizeType inBucket) const;
	LocalIterator end(SizeType inBucket);
	LocalConstIterator end(SizeType inBucket) const;
	LocalConstIterator cend(SizeType inBucket) const;

private:
	UnorderedMap mUnorderedMap;
};

#include "DX12Game/ContainerUtil.inl"