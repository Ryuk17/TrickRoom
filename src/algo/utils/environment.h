/*
 * Minimal FieldTrialsView stub for the pure build.
 * Always returns default values — no runtime experiment overrides.
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

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_H_
