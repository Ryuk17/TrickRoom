/*
 * Minimal ApmDataDumper stub for the pure build.
 * All dump methods are no-ops (debug dump disabled).
 */
#ifndef MODULES_AUDIO_PROCESSING_LOGGING_APM_DATA_DUMPER_H_
#define MODULES_AUDIO_PROCESSING_LOGGING_APM_DATA_DUMPER_H_

#include <cstddef>

#include "absl/strings/string_view.h"
#include "utils/array_view.h"

namespace webrtc {

class ApmDataDumper {
 public:
  explicit ApmDataDumper(int /*instance_index*/) {}
  ~ApmDataDumper() = default;
  ApmDataDumper(const ApmDataDumper&) = delete;
  ApmDataDumper& operator=(const ApmDataDumper&) = delete;

  static void SetActivated(bool /*activated*/) {}
  static bool IsAvailable() { return false; }
  static constexpr size_t kDefaultDumpSet = 0;
  static void SetDumpSetToUse(int /*dump_set_to_use*/) {}
  static void SetOutputDirectory(absl::string_view /*output_dir*/) {}
  void InitiateNewSetOfRecordings() {}

  // Catch-all template for scalar values (int, float, double, etc.)
  template <typename T>
  void DumpRaw(absl::string_view /*name*/, T /*v*/,
               int /*dump_set*/ = kDefaultDumpSet) {}
  // Array overloads (matching the original API signatures)
  void DumpRaw(absl::string_view /*name*/, size_t /*v_length*/,
               const double* /*v*/, int /*dump_set*/ = kDefaultDumpSet) {}
  void DumpRaw(absl::string_view /*name*/, size_t /*v_length*/,
               const float* /*v*/, int /*dump_set*/ = kDefaultDumpSet) {}
  void DumpRaw(absl::string_view /*name*/, ArrayView<const double> /*v*/,
               int /*dump_set*/ = kDefaultDumpSet) {}
  void DumpRaw(absl::string_view /*name*/, ArrayView<const float> /*v*/,
               int /*dump_set*/ = kDefaultDumpSet) {}
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LOGGING_APM_DATA_DUMPER_H_
