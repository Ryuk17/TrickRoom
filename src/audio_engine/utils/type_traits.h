#ifndef RTC_BASE_TYPE_TRAITS_H_
#define RTC_BASE_TYPE_TRAITS_H_

#include <type_traits>

namespace webrtc {

// Helper for ArrayView construction: checks that U has data() and size()
// methods that are compatible with T.
template <typename U, typename T>
struct HasDataAndSize {
 private:
  template <typename C, typename = decltype(std::declval<C>().data()),
            typename = decltype(std::declval<C>().size())>
  static std::true_type test(int);
  template <typename>
  static std::false_type test(...);

 public:
  static constexpr bool value =
      decltype(test<std::remove_reference_t<U>>(0))::value;
};

}  // namespace webrtc

#endif  // RTC_BASE_TYPE_TRAITS_H_
