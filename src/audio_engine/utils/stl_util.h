/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/stl_util.h.
// Adapted for audio_engine: include paths changed.

#ifndef UTILS_STL_UTIL_H_
#define UTILS_STL_UTIL_H_

#include <assert.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

namespace webrtc {

// Clears internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
template<class T>
void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  // Sometimes "T tmp" allocates objects with memory (arena implementation?).
  // Hence using additional reserve(0) even if it doesn't always work.
  obj->reserve(0);
}

// For a range within a container of pointers, calls delete (non-array version)
// on these pointers.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin, ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// For a range within a container of pairs, calls delete (non-array version) on
// BOTH items in the pairs.
template <class ForwardIterator>
void STLDeleteContainerPairPointers(ForwardIterator begin,
                                    ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
    delete temp->second;
  }
}

// For a range within a container of pairs, calls delete (non-array version) on
// the FIRST item in the pairs.
template <class ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
  }
}

// For a range within a container of pairs, calls delete.
template <class ForwardIterator>
void STLDeleteContainerPairSecondPointers(ForwardIterator begin,
                                          ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->second;
  }
}

// To treat a possibly-empty vector as an array, use these functions.
template<typename T>
inline T* vector_as_array(std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

template<typename T>
inline const T* vector_as_array(const std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

// Return a mutable char* pointing to a string's internal buffer.
inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.
template <class T>
void STLDeleteElements(T* container) {
  if (!container)
    return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.
template <class T>
void STLDeleteValues(T* container) {
  if (!container)
    return;
  for (typename T::iterator i(container->begin()); i != container->end(); ++i)
    delete i->second;
  container->clear();
}

// Test to see if a set, map, hash_set or hash_map contains a particular key.
template <typename Collection, typename Key>
bool ContainsKey(const Collection& collection, const Key& key) {
  return collection.find(key) != collection.end();
}

// Returns true if the container is sorted.
template <typename Container>
bool STLIsSorted(const Container& cont) {
  return std::adjacent_find(cont.rbegin(), cont.rend(),
                            std::less<typename Container::value_type>())
      == cont.rend();
}

// Returns a new ResultType containing the difference of two sorted containers.
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetDifference(const Arg1& a1, const Arg2& a2) {
  assert(STLIsSorted(a1));
  assert(STLIsSorted(a2));
  ResultType difference;
  std::set_difference(a1.begin(), a1.end(),
                      a2.begin(), a2.end(),
                      std::inserter(difference, difference.end()));
  return difference;
}

// Returns a new ResultType containing the union of two sorted containers.
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetUnion(const Arg1& a1, const Arg2& a2) {
  assert(STLIsSorted(a1));
  assert(STLIsSorted(a2));
  ResultType result;
  std::set_union(a1.begin(), a1.end(),
                 a2.begin(), a2.end(),
                 std::inserter(result, result.end()));
  return result;
}

// Returns a new ResultType containing the intersection of two sorted containers.
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetIntersection(const Arg1& a1, const Arg2& a2) {
  assert(STLIsSorted(a1));
  assert(STLIsSorted(a2));
  ResultType result;
  std::set_intersection(a1.begin(), a1.end(),
                        a2.begin(), a2.end(),
                        std::inserter(result, result.end()));
  return result;
}

// Returns true if the sorted container |a1| contains all elements of the sorted
// container |a2|.
template <typename Arg1, typename Arg2>
bool STLIncludes(const Arg1& a1, const Arg2& a2) {
  assert(STLIsSorted(a1));
  assert(STLIsSorted(a2));
  return std::includes(a1.begin(), a1.end(),
                       a2.begin(), a2.end());
}

}  // namespace webrtc

#endif  // UTILS_STL_UTIL_H_
