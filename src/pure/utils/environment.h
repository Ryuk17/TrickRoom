/*
 * Minimal Environment + FieldTrialsView for the pure build.
 * Only field_trials() is used by AEC3. All other utilities are no-op stubs.
 */
#ifndef API_ENVIRONMENT_ENVIRONMENT_H_
#define API_ENVIRONMENT_ENVIRONMENT_H_

#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

class FieldTrialsView {
 public:
  FieldTrialsView() = default;
  bool IsEnabled(absl::string_view /*trial*/) const { return false; }
  bool IsDisabled(absl::string_view trial) const { return !IsEnabled(trial); }
  std::string Lookup(absl::string_view /*key*/) const { return ""; }
};

class Environment {
 public:
  Environment() = default;

  const FieldTrialsView& field_trials() const { return field_trials_; }

 private:
  FieldTrialsView field_trials_;
};

inline Environment CreateEnvironment() { return Environment(); }

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_H_
