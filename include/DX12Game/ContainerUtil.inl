#ifndef __CONTAINERUTIL_INL__
#define __CONTAINERUTIL_INL__

#include "StringUtil.h"

///
// ======== GARRAY ================================
///
// Element access
///
GARRAY_TEMPLATE
typename GARRAY::Reference GARRAY::at(SizeType inPos) {
	return mArray.at(inPos);
}

GARRAY_TEMPLATE
constexpr typename GARRAY::ConstReference GARRAY::at(SizeType inPos) const {
	return mArray.at(inPos);
}

GARRAY_TEMPLATE
typename GARRAY::Reference GARRAY::operator[](SizeType inPos) noexcept {
	return mArray[inPos];
}

GARRAY_TEMPLATE
constexpr typename GARRAY::ConstReference GARRAY::operator[](SizeType inPos) const {
	return mArray[inPos];
}

GARRAY_TEMPLATE
typename GARRAY::Reference GARRAY::front() {
	return mArray.front();
}

GARRAY_TEMPLATE
constexpr typename GARRAY::ConstReference GARRAY::front() const {
	return mArray.front();
}

GARRAY_TEMPLATE
typename GARRAY::Reference GARRAY::back() {
	return mArray.back();
}

GARRAY_TEMPLATE
constexpr typename GARRAY::ConstReference GARRAY::back() const {
	return mArray.back();
}

GARRAY_TEMPLATE
T* GARRAY::data() noexcept {
	return mArray.data();
}

GARRAY_TEMPLATE
const T* GARRAY::data() const noexcept {
	return mArray.data();
}
/// Element access

///
//  Iterators
///
GARRAY_TEMPLATE
GARRAY_ITERATOR GARRAY::begin() noexcept {
	return mArray.begin();
}

GARRAY_TEMPLATE
GARRAY_CONST_ITERATOR GARRAY::begin() const noexcept {
	return mArray.begin();
}

GARRAY_TEMPLATE
GARRAY_CONST_ITERATOR GARRAY::cbegin() const noexcept {
	return mArray.cbegin();
}

GARRAY_TEMPLATE
GARRAY_ITERATOR GARRAY::end() noexcept {
	return mArray.end();
}

GARRAY_TEMPLATE
GARRAY_CONST_ITERATOR GARRAY::end() const noexcept {
	return mArray.end();
}

GARRAY_TEMPLATE
GARRAY_CONST_ITERATOR GARRAY::cend() const noexcept {
	return mArray.cend();
}

GARRAY_TEMPLATE
GARRAY_REVERSE_ITERATOR GARRAY::rbegin() noexcept {
	return mArray.rbegin();
}

GARRAY_TEMPLATE
GARRAY_CONST_REVERSE_ITERATOR GARRAY::rbegin() const noexcept {
	return mArray.rbegin();
}

GARRAY_TEMPLATE
GARRAY_CONST_REVERSE_ITERATOR GARRAY::crbegin() const noexcept {
	return mArray.crbegin();
}

GARRAY_TEMPLATE
GARRAY_REVERSE_ITERATOR GARRAY::rend() noexcept {
	return mArray.rend();
}

GARRAY_TEMPLATE
GARRAY_CONST_REVERSE_ITERATOR GARRAY::rend() const noexcept {
	return mArray.rend();
}

GARRAY_TEMPLATE
GARRAY_CONST_REVERSE_ITERATOR GARRAY::crend() const noexcept {
	return mArray.crend();
}
///  Iterators

///
// Capacity
///
GARRAY_TEMPLATE
constexpr bool GARRAY::empty() const noexcept {
	return mArray.empty();
}

GARRAY_TEMPLATE
constexpr GARRAY_SIZE_TYPE GARRAY::size() const noexcept {
	return mArray.size();
}

GARRAY_TEMPLATE
constexpr GARRAY_SIZE_TYPE GARRAY::max_size() const noexcept {
	return mArray.max_size();
}
/// Capacity

///
// Operations
///
GARRAY_TEMPLATE
void GARRAY::fill(const T& inValue) {
	mArray.fill(inValue);
}

GARRAY_TEMPLATE
void GARRAY::swap(Array& inOther) noexcept {
	mArray.swap(inOther);
}
/// Operations
/// ======== GARRAY ================================

///
//  ======== GVECTOR ================================
///
// Constructions
///
GVECTOR_TEMPLATE
GVECTOR::GVector(SizeType inSize) {
	mVector.resize(inSize);
}

GVECTOR_TEMPLATE
GVECTOR::GVector(InitializerList inList) {
	mVector.insert(mVector.begin(), inList.begin(), inList.end());
}
/// Constructions

///
// Member functions
///
GVECTOR_TEMPLATE
typename GVECTOR& GVECTOR::operator=(InitializerList inList) {
	mVector = inList;
	return (*this);
}

GVECTOR_TEMPLATE
void GVECTOR::assgin(SizeType inCount, ConstReference inValue) {
	mVector.assign(inCount, inValue);
}

GVECTOR_TEMPLATE
template <typename InputIt> 
void GVECTOR::assign(InputIt inFirst, InputIt inLast) {
	mVector.assign(inFirst, inLast);
}

GVECTOR_TEMPLATE
void GVECTOR::assgin(InitializerList inIList) {
	mVector.assign(inIList);
}

GVECTOR_TEMPLATE
GVECTOR_ALLOCATOR_TYPE GVECTOR::get_allocator() const noexcept {
	return mVector.get_allocator();
}
/// Member functions

///
// Element access
///
GVECTOR_TEMPLATE
GVECTOR_REFERENCE GVECTOR::at(SizeType inPos) {
	return const_cast<T&>(std::as_const(*this).at(inPos));
}

GVECTOR_TEMPLATE
constexpr GVECTOR_CONST_REFERENCE GVECTOR::at(SizeType inPos) const {
	if (inPos < 0 || inPos >= mVector.size()) {
		std::stringstream message;
		message << FileLineStr << "Out of range: inPos >= 0 && inPos < vector.size()" << std::endl;
		throw std::invalid_argument(message.str().c_str());
	}

	return mVector.at(inPos);
}

GVECTOR_TEMPLATE
GVECTOR_REFERENCE GVECTOR::operator[](SizeType inIdx) {
	return const_cast<Reference>(std::as_const(*this).operator[](inIdx));
}

GVECTOR_TEMPLATE
constexpr GVECTOR_CONST_REFERENCE GVECTOR::operator[](SizeType inIdx) const {
	if (inIdx < 0 || inIdx >= mVector.size()) {
		std::stringstream message;
		message << FileLineStr << "Out of range: inIdx >= 0 && inIdx < vector.size()" << std::endl;
		throw std::invalid_argument(message.str().c_str());
	}
	
	return mVector.operator[](inIdx);
}

GVECTOR_TEMPLATE
GVECTOR_REFERENCE GVECTOR::front() {
	return const_cast<T&>(std::as_const(*this).front());
}

GVECTOR_TEMPLATE
constexpr GVECTOR_CONST_REFERENCE GVECTOR::front() const {
	if (mVector.size() == 0) {
		std::stringstream message;
		message << FileLineStr << "Vector size is 0" << std::endl;
		throw std::logic_error(message.str().c_str());
	}

	return mVector.front();
}

GVECTOR_TEMPLATE
GVECTOR_REFERENCE GVECTOR::back() {
	return const_cast<T&>(std::as_const(*this).back());
}

GVECTOR_TEMPLATE
constexpr GVECTOR_CONST_REFERENCE GVECTOR::back() const {
	if (mVector.size() == 0) {
		std::stringstream message;
		message << FileLineStr << "Vector size is 0" << std::endl;
		throw std::logic_error(message.str().c_str());
	}

	return mVector.back();
}

GVECTOR_TEMPLATE
T* GVECTOR::data() noexcept {
	return const_cast<T*>(std::as_const(*this).data());
}
/// Element access

GVECTOR_TEMPLATE
const T* GVECTOR::data() const noexcept {
	return mVector.data();
}

///
// Iterators 
///
GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::begin() noexcept {
	return mVector.begin();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_ITERATOR GVECTOR::begin() const noexcept {
	return mVector.begin();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_ITERATOR GVECTOR::cbegin() const noexcept {
	return mVector.cbegin();
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::end() noexcept {
	return mVector.end();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_ITERATOR GVECTOR::end() const noexcept {
	return mVector.end();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_ITERATOR GVECTOR::cend() const noexcept {
	return mVector.cend();
}

GVECTOR_TEMPLATE
GVECTOR_REVERSE_ITERATOR GVECTOR::rbegin() noexcept {
	return mVector.rbegin();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_REVERSE_ITERATOR GVECTOR::crbegin() const noexcept {
	return mVector.crbegin();
}

GVECTOR_TEMPLATE
GVECTOR_REVERSE_ITERATOR GVECTOR::rend() noexcept {
	return mVector.rend();
}

GVECTOR_TEMPLATE
GVECTOR_CONST_REVERSE_ITERATOR GVECTOR::crend() const noexcept {
	return mVector.crend();
}
/// Iterators 

///
// Capacity
///
GVECTOR_TEMPLATE
bool GVECTOR::empty() const noexcept {
	return mVector.empty();
}

GVECTOR_TEMPLATE
typename GVECTOR::SizeType GVECTOR::size() const noexcept {
	return mVector.size();
}

GVECTOR_TEMPLATE
typename GVECTOR::SizeType GVECTOR::max_size() const noexcept {
	return mVector.max_size();
}

GVECTOR_TEMPLATE
void GVECTOR::reserve(SizeType inNewCapacity) {
	mVector.reserve(inNewCapacity);
}

GVECTOR_TEMPLATE
typename GVECTOR::SizeType GVECTOR::capacity() const noexcept {
	return mVector.capacity();
}

GVECTOR_TEMPLATE
void GVECTOR::shrink_to_fit() {
	mVector.shrink_to_fit();
}
/// Capacity

///
// Modifiers
///
GVECTOR_TEMPLATE
void GVECTOR::clear() noexcept {
	mVector.clear();
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::insert(ConstIterator inPos, ConstReference inValue) {
	return mVector.emplace(inPos, inValue);
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::insert(ConstIterator inPos, RightValue inValue) {
	return mVector.emplace(inPos, std::move(inValue));
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::insert(ConstIterator inPos, const SizeType inCount, ConstReference inValue) {
	return mVector.insert(inPos, inCount, inValue);
}

GVECTOR_TEMPLATE
template <class InputIt>
GVECTOR_ITERATOR GVECTOR::insert(ConstIterator inPos, InputIt inFirst, InputIt inLast) {
	return mVector.insert(inPos, inFirst, inLast);
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::insert(ConstIterator inPos, InitializerList inList) {
	return mVector.insert(inPos, inList.begin(), inList.end());
}

GVECTOR_TEMPLATE
template <typename... Args>	
GVECTOR_ITERATOR GVECTOR::emplace(ConstIterator inPos, Args&&... inArgs) {
	return mVector.emplace(inPos, std::forward<Args>(inArgs)...);
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::erase(ConstIterator inPos) {
	if (inPos < mVector.cbegin() || inPos >= mVector.cend()) {
		WErrln(L"Out of range: inPos >= begin() && inPos < end()");
		return mVector.end();
	}

	return mVector.erase(inPos);
}

GVECTOR_TEMPLATE
GVECTOR_ITERATOR GVECTOR::erase(ConstIterator inFirst, ConstIterator inLast) {
	if ((inFirst < mVector.cbegin() || inFirst >= mVector.cend()) ||
			(inLast < mVector.cbegin() || inLast >= mVector.cend())) {
		WErrln(L"Out of range: inPos >= begin() && inPos < end()");
		return mVector.end();
	}
	else if (inFirst > inLast) {
		WErrln(L"Error of iterators: inFirst <= inLast");
		return mVector.end();
	}

	return mVector.erase(inFirst, inLast);
}

GVECTOR_TEMPLATE
void GVECTOR::push_back(ConstReference inValue) {
	mVector.emplace_back(inValue);
}

GVECTOR_TEMPLATE
void GVECTOR::push_back(RightValue inValue) {
	mVector.emplace_back(std::move(inValue));
}

GVECTOR_TEMPLATE
template <typename... Args>
decltype(auto) GVECTOR::emplace_back(Args&&... inArgs) {
	return mVector.emplace_back(std::forward<Args>(inArgs)...);
}

GVECTOR_TEMPLATE
void GVECTOR::pop_back() {
	mVector.pop_back();
}

GVECTOR_TEMPLATE
void GVECTOR::resize(SizeType inCount) {
	mVector.resize(inCount);
}

GVECTOR_TEMPLATE
void GVECTOR::resize(SizeType inCount, ConstReference inValue) {
	mVector.resize(inCount, inValue);
}

GVECTOR_TEMPLATE
void GVECTOR::swap(GVector& inOther) {
	mVector.swap(inOther.mVector);
}
/// Modifiers
///  ======== GVECTOR ================================

///
//  ======== GVECTOR for bool ================================
///
// Constructions
///
GVECTOR_BOOL::GVector(SizeType inSize) {
	mVector.resize(inSize);
}

GVECTOR_BOOL::GVector(InitializerList inList) {
	mVector.insert(mVector.begin(), inList.begin(), inList.end());
}
/// Constructions

GVECTOR_BOOL& GVECTOR_BOOL::operator=(InitializerList inList) {
	mVector = inList;
	return (*this);
}

void GVECTOR_BOOL::assgin(SizeType inCount, ConstReference inValue) {
	mVector.assign(inCount, inValue);
}

template <typename InputIt> 
void GVECTOR_BOOL::assign(InputIt inFirst, InputIt inLast) {
	mVector.assign(inFirst, inLast);
}

void GVECTOR_BOOL::assgin(InitializerList inIList) {
	mVector.assign(inIList);
}

GVECTOR_BOOL::AllocatorType GVECTOR_BOOL::get_allocator() const noexcept {
	return mVector.get_allocator();
}

///
// Element access
///
GVECTOR_BOOL::Reference GVECTOR_BOOL::at(SizeType inPos) {
	return const_cast<bool&>(std::as_const(*this).at(inPos));
}

GVECTOR_BOOL::ConstReference GVECTOR_BOOL::at(SizeType inPos) const {
	if (inPos < 0 || inPos >= mVector.size()) {
		std::stringstream message;
		message << FileLineStr << "Out of range: inPos >= 0 && inPos < vector.size()" << std::endl;
		throw std::invalid_argument(message.str().c_str());
	}

	return mVector.at(inPos);
}

GVECTOR_BOOL::Reference GVECTOR_BOOL::operator[](SizeType inIdx) {
	return const_cast<Reference>(std::as_const(*this).operator[](inIdx));
}

GVECTOR_BOOL::ConstReference GVECTOR_BOOL::operator[](SizeType inIdx) const {
	if (inIdx < 0 || inIdx >= mVector.size()) {
		std::stringstream message;
		message << FileLineStr << "Out of range: inIdx >= 0 && inIdx < vector.size()" << std::endl;
		throw std::invalid_argument(message.str().c_str());
	}

	return mVector.operator[](inIdx);
}

GVECTOR_BOOL::Reference GVECTOR_BOOL::front() {
	return const_cast<bool&>(std::as_const(*this).front());
}

GVECTOR_BOOL::ConstReference GVECTOR_BOOL::front() const {
	if (mVector.size() == 0) {
		std::stringstream message;
		message << FileLineStr << "Vector size is 0" << std::endl;
		throw std::logic_error(message.str().c_str());
	}

	return mVector.front();
}

GVECTOR_BOOL::Reference GVECTOR_BOOL::back() {
	return const_cast<bool&>(std::as_const(*this).back());
}

GVECTOR_BOOL::ConstReference GVECTOR_BOOL::back() const {
	if (mVector.size() == 0) {
		std::stringstream message;
		message << FileLineStr << "Vector size is 0" << std::endl;
		throw std::logic_error(message.str().c_str());
	}

	return mVector.back();
}
/// Element access

///
// Iterators
///
GVECTOR_BOOL::Iterator GVECTOR_BOOL::begin() noexcept {
	return mVector.begin();
}

GVECTOR_BOOL::ConstIterator GVECTOR_BOOL::begin() const noexcept {
	return mVector.begin();
}

GVECTOR_BOOL::ConstIterator GVECTOR_BOOL::cbegin() const noexcept {
	return mVector.cbegin();
}

GVECTOR_BOOL::Iterator GVECTOR_BOOL::end() noexcept {
	return mVector.end();
}

GVECTOR_BOOL::ConstIterator GVECTOR_BOOL::end() const noexcept {
	return mVector.end();
}

GVECTOR_BOOL::ConstIterator GVECTOR_BOOL::cend() const noexcept {
	return mVector.cend();
}

GVECTOR_BOOL::ReverseIterator GVECTOR_BOOL::rbegin() noexcept {
	return mVector.rbegin();
}

GVECTOR_BOOL::ConstReverseIterator GVECTOR_BOOL::rbegin() const noexcept {
	return mVector.rbegin();
}

GVECTOR_BOOL::ConstReverseIterator GVECTOR_BOOL::crbegin() const noexcept {
	return mVector.crbegin();
}

GVECTOR_BOOL::ReverseIterator GVECTOR_BOOL::rend() noexcept {
	return mVector.rend();
}

GVECTOR_BOOL::ConstReverseIterator GVECTOR_BOOL::rend() const noexcept {
	return mVector.rend();
}

GVECTOR_BOOL::ConstReverseIterator GVECTOR_BOOL::crend() const noexcept {
	return mVector.crend();
}
/// Iterators

///
// Capacity
///
bool GVECTOR_BOOL::empty() const noexcept {
	return mVector.empty();
}

GVECTOR_BOOL::SizeType GVECTOR_BOOL::size() const noexcept {
	return mVector.size();
}

GVECTOR_BOOL::SizeType GVECTOR_BOOL::max_size() const noexcept {
	return mVector.max_size();
}

void GVECTOR_BOOL::reserve(SizeType inNewCapacity) {
	mVector.reserve(inNewCapacity);
}

GVECTOR_BOOL::SizeType GVECTOR_BOOL::capacity() const noexcept {
	return mVector.capacity();
}

void GVECTOR_BOOL::shrink_to_fit() {
	mVector.shrink_to_fit();
}
/// Capacity

///
// Modifiers
///
void GVECTOR_BOOL::clear() noexcept {
	mVector.clear();
}

GVECTOR_BOOL::Iterator GVECTOR_BOOL::insert(ConstIterator inPos, ConstReference inValue) {
	return mVector.insert(inPos, inValue); 
}

//Iterator insert(ConstIterator inPos, RightValue inValue);

GVECTOR_BOOL::Iterator GVECTOR_BOOL::insert(ConstIterator inPos, const SizeType inCount, ConstReference inValue) {
	return mVector.insert(inPos, inCount, std::move(inValue));
}

template <class InputIt> 
GVECTOR_BOOL::Iterator GVECTOR_BOOL::insert(ConstIterator inPos, InputIt inFirst, InputIt inLast) {
	return mVector.insert(inPos, inFirst, inLast);
}

GVECTOR_BOOL::Iterator GVECTOR_BOOL::insert(ConstIterator inPos, InitializerList inList) {
	return mVector.insert(inPos, inList.begin(), inList.end());
}

template <typename... Args>	
GVECTOR_BOOL::Iterator GVECTOR_BOOL::emplace(ConstIterator inPos, Args&&... inArgs) {
	return mVector.emplace(inPos, std::forward<Args>(inArgs)...);
}

GVECTOR_BOOL::Iterator GVECTOR_BOOL::erase(ConstIterator inPos) {
	if (inPos < mVector.cbegin() || inPos >= mVector.cend()) {
		WErrln(L"Out of range: inPos >= begin() && inPos < end()");
		return mVector.end();
	}

	return mVector.erase(inPos);
}

GVECTOR_BOOL::Iterator GVECTOR_BOOL::erase(ConstIterator inFirst, ConstIterator inLast) {
	if ((inFirst < mVector.cbegin() || inFirst >= mVector.cend()) ||
		(inLast < mVector.cbegin() || inLast >= mVector.cend())) {
		WErrln(L"Out of range: inPos >= begin() && inPos < end()");
		return mVector.end();
	}
	else if (inFirst > inLast) {
		WErrln(L"Error of iterators: inFirst <= inLast");
		return mVector.end();
	}

	return mVector.erase(inFirst, inLast);
}

void GVECTOR_BOOL::push_back(ConstReference inValue) {
	mVector.emplace_back(inValue);
}

//void push_back(RightValue inValue);

template <typename... Args>	
decltype(auto) GVECTOR_BOOL::emplace_back(Args&&... inArgs) {
	return mVector.emplace_back(std::forward<Args>(inArgs)...);
}

void GVECTOR_BOOL::pop_back() {
	mVector.pop_back();
}

void GVECTOR_BOOL::resize(SizeType inCount) {
	mVector.resize(inCount);
}

void GVECTOR_BOOL::resize(SizeType inCount, ConstReference inValue) {
	mVector.resize(inCount, inValue);
}

void GVECTOR_BOOL::swap(GVector& inOther) {
	mVector.swap(inOther.mVector);
}
/// Modifiers
///  ======== GVECTOR for bool ================================

///
// ======== GUNORDERED ================================
///
// Constructions
///
UNORDERED_TEMPLATE
typename UNORDEREDMAP& UNORDEREDMAP::operator=(const GUnorderedMap& inOther) {
	mUnorderedMap = inOther.mUnorderedMap;
	return (*this);
}

UNORDERED_TEMPLATE
typename UNORDEREDMAP& UNORDEREDMAP::operator=(GUnorderedMap&& inOther) {
	mUnorderedMap = std::move(inOther.mUnorderedMap);
	return (*this);
}
/// Constructions

///
// Iterators
///
UNORDERED_TEMPLATE
UNORDERED_ITERATOR UNORDEREDMAP::begin() noexcept {
	return mUnorderedMap.begin();
}

UNORDERED_TEMPLATE
UNORDERED_CONST_ITERATOR UNORDEREDMAP::begin() const noexcept {
	return mUnorderedMap.begin();
}

UNORDERED_TEMPLATE
UNORDERED_CONST_ITERATOR UNORDEREDMAP::cbegin() const noexcept {
	return mUnorderedMap.cbegin();
}

UNORDERED_TEMPLATE
UNORDERED_ITERATOR UNORDEREDMAP::end() noexcept {
	return mUnorderedMap.end();
}

UNORDERED_TEMPLATE
UNORDERED_CONST_ITERATOR UNORDEREDMAP::end() const noexcept {
	return mUnorderedMap.end();
}

UNORDERED_TEMPLATE
UNORDERED_CONST_ITERATOR UNORDEREDMAP::cend() const noexcept {
	return mUnorderedMap.cend();
}
/// Iterators

///
// Capacity
///
UNORDERED_TEMPLATE
bool UNORDEREDMAP::empty() const noexcept {
	return mUnorderedMap.empty();
}

UNORDERED_TEMPLATE
typename UNORDEREDMAP::SizeType UNORDEREDMAP::size() const noexcept {
	return mUnorderedMap.size();
}

UNORDERED_TEMPLATE
typename UNORDEREDMAP::SizeType UNORDEREDMAP::max_size() const noexcept {
	return mUnorderedMap.max_size();
}
/// Capacity


///
// Modifiers
///
UNORDERED_TEMPLATE
void UNORDEREDMAP::clear() noexcept {
	mUnorderedMap.clear();
}

UNORDERED_TEMPLATE
template <typename... Args> 
std::pair<UNORDERED_ITERATOR, bool> UNORDEREDMAP::emplace(Args&&... inArgs) {
	return mUnorderedMap.emplace(std::forward<Args>(inArgs)...);
}

UNORDERED_TEMPLATE
template <typename... Args> 
UNORDERED_ITERATOR UNORDEREDMAP::emplace_hint(ConstIterator inHint, Args&&... inArgs) {
	return mUnorderedMap.emplace_hint(std::forward<Args>(inArgs)...);
}

UNORDERED_TEMPLATE
UNORDERED_ITERATOR UNORDEREDMAP::erase(ConstIterator inPos) {
	return mUnorderedMap.erase(inPos);
}

UNORDERED_TEMPLATE
UNORDERED_ITERATOR UNORDEREDMAP::erase(ConstIterator inFirst, ConstIterator inLast) {
	return mUnorderedMap.erase(inFirst, inLast);
}

UNORDERED_TEMPLATE
typename UNORDEREDMAP::SizeType UNORDEREDMAP::erase(const KeyType& inKey) {
	return mUnorderedMap.erase(inKey);
}
/// Modifiers

///
// Lookup
///
UNORDERED_TEMPLATE
ValType& UNORDEREDMAP::at(const KeyType& inKey) {
	return const_cast<ValType&>(std::as_const(*this).at(inKey));
}

UNORDERED_TEMPLATE
const ValType& UNORDEREDMAP::at(const KeyType& inKey) const {
	auto iter = mUnorderedMap.find(inKey);
	if (iter != mUnorderedMap.end()) {
		std::stringstream message;
		message << FileLineStr << "Out of range: invalid key" << std::endl;
		throw std::invalid_argument(message.str().c_str());
	}

	return mUnorderedMap.at(inKey);
}

UNORDERED_TEMPLATE
ValType& UNORDEREDMAP::operator[](const KeyType& inKey) {
	return mUnorderedMap.try_emplace(inKey).first->second;
}

UNORDERED_TEMPLATE
ValType& UNORDEREDMAP::operator[](KeyType&& inKey) {
	return mUnorderedMap.try_emplace(std::move(inKey)).first->second;
}

UNORDERED_TEMPLATE
typename UNORDEREDMAP::SizeType UNORDEREDMAP::count(const KeyType& inKey) const {
	return mUnorderedMap.count(inKey);
}

UNORDERED_TEMPLATE
UNORDERED_ITERATOR UNORDEREDMAP::find(const KeyType& inKey) {
	return mUnorderedMap.lower_bound(inKey);
}

UNORDERED_TEMPLATE
UNORDERED_CONST_ITERATOR UNORDEREDMAP::find(const KeyType& inKey) const {
	return mUnorderedMap.lower_bound(inKey);
}

UNORDERED_TEMPLATE
std::pair<UNORDERED_ITERATOR, UNORDERED_ITERATOR> UNORDEREDMAP::equal_range(const KeyType& inKey) {
	return mUnorderedMap.equal_range(inKey);
}

UNORDERED_TEMPLATE
std::pair<UNORDERED_CONST_ITERATOR, UNORDERED_CONST_ITERATOR> UNORDEREDMAP::equal_range(const KeyType& inKey) const {
	return mUnorderedMap.equal_range(inKey);
}
/// Lookup

///
// Bucket interface
///
UNORDERED_TEMPLATE
UNORDERED_LOCAL_ITERATOR UNORDEREDMAP::begin(SizeType inBucket) {
	return mUnorderedMap.begin(inBucket);
}

UNORDERED_TEMPLATE
UNORDERED_LOCAL_CONST_ITERATOR UNORDEREDMAP::begin(SizeType inBucket) const {
	return mUnorderedMap.begin(inBucket);
}

UNORDERED_TEMPLATE
UNORDERED_LOCAL_CONST_ITERATOR UNORDEREDMAP::cbegin(SizeType inBucket) const {
	return mUnorderedMap.cbegin(inBucket);
}

UNORDERED_TEMPLATE
UNORDERED_LOCAL_ITERATOR UNORDEREDMAP::end(SizeType inBucket) {
	return mUnorderedMap.end(inBucket);
}

UNORDERED_TEMPLATE
UNORDERED_LOCAL_CONST_ITERATOR UNORDEREDMAP::end(SizeType inBucket) const {
	return mUnorderedMap.end(inBucket);
}

UNORDERED_TEMPLATE
UNORDERED_LOCAL_CONST_ITERATOR UNORDEREDMAP::cend(SizeType inBucket) const {
	return mUnorderedMap.cend(inBucket);
}
/// Bucket interface

#endif // __CONTAINERUTIL_INL__