/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Adapted from webrtc/base/optional.h for audio_engine.
// Include path changed from webrtc/base/checks.h to utils/checks.h.

#ifndef UTILS_OPTIONAL_WEBRTC_H_
#define UTILS_OPTIONAL_WEBRTC_H_

#include <algorithm>
#include <utility>

#include "utils/checks.h"

namespace rtc {

// Simple std::experimental::optional-wannabe. It either contains a T or not.
template <typename T>
class Optional final {
 public:
  // Construct an empty Optional.
  Optional() : has_value_(false) {}

  // Construct an Optional that contains a value.
  explicit Optional(const T& val) : value_(val), has_value_(true) {}
  explicit Optional(T&& val) : value_(std::move(val)), has_value_(true) {}

  // Copy and move constructors.
  Optional(const Optional&) = default;
  Optional(Optional&& m)
      : value_(std::move(m.value_)), has_value_(m.has_value_) {}

  // Assignment.
  Optional& operator=(const Optional&) = default;
  Optional& operator=(Optional&& m) {
    value_ = std::move(m.value_);
    has_value_ = m.has_value_;
    return *this;
  }

  friend void swap(Optional& m1, Optional& m2) {
    using std::swap;
    swap(m1.value_, m2.value_);
    swap(m1.has_value_, m2.has_value_);
  }

  // Conversion to bool to test if we have a value.
  explicit operator bool() const { return has_value_; }

  // Dereferencing. Only allowed if we have a value.
  const T* operator->() const {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  T* operator->() {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  const T& operator*() const {
    RTC_DCHECK(has_value_);
    return value_;
  }
  T& operator*() {
    RTC_DCHECK(has_value_);
    return value_;
  }

  // Dereference with a default value in case we don't have a value.
  const T& value_or(const T& default_val) const {
    return has_value_ ? value_ : default_val;
  }

  // Equality tests. Two Optionals are equal if they contain equivalent values,
  // or if they're both empty.
  friend bool operator==(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ == m2.value_
                                          : m1.has_value_ == m2.has_value_;
  }
  friend bool operator!=(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ != m2.value_
                                          : m1.has_value_ != m2.has_value_;
  }

 private:
  // Invariant: Unless *this has been moved from, value_ is default-initialized
  // (or copied or moved from a default-initialized T) if !has_value_.
  T value_;
  bool has_value_;
};

}  // namespace rtc

#endif  // UTILS_OPTIONAL_WEBRTC_H_
