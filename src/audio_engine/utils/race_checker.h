/*
 * Minimal RaceChecker stub (no-op in pure build, single-threaded).
 */
#ifndef RTC_BASE_RACE_CHECKER_H_
#define RTC_BASE_RACE_CHECKER_H_

namespace webrtc {
namespace race_checker_internal {

class RaceCheckerScope {};

}  // namespace race_checker_internal

#define RTC_DCHECK_RUNS_SERIALIZED(...) ((void)0)

class RaceChecker {
 public:
  RaceChecker() = default;
  bool IsCurrent() const { return true; }
};

}  // namespace webrtc

#endif  // RTC_BASE_RACE_CHECKER_H_
