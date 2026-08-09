/*
 * Minimal FieldTrialParameter stub (no-op in pure build).
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_

#include <initializer_list>
#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

class FieldTrialParameterInterface {
 public:
  explicit FieldTrialParameterInterface(absl::string_view /*key*/) {}
  virtual ~FieldTrialParameterInterface() = default;
};

template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(absl::string_view key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }
  operator T() const { return Get(); }

 private:
  T value_;
};

inline void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> /*fields*/,
    absl::string_view /*trial_string*/) {}

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
