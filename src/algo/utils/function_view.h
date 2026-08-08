/*
 * Minimal FunctionView for the pure build.
 * Extracted from api/function_view.h.
 */
#ifndef API_FUNCTION_VIEW_H_
#define API_FUNCTION_VIEW_H_

#include <cstddef>
#include <type_traits>
#include <utility>

namespace webrtc {

template <typename T>
class FunctionView;  // Undefined.

template <typename RetT, typename... ArgT>
class FunctionView<RetT(ArgT...)> final {
 public:
  template <typename F,
            typename std::enable_if<
                !std::is_function<typename std::remove_pointer<
                    typename std::remove_reference<F>::type>::type>::value &&
                !std::is_same<std::nullptr_t,
                              typename std::remove_cv<F>::type>::value &&
                !std::is_same<FunctionView,
                              typename std::remove_cv<
                                  typename std::remove_reference<F>::type>::
                                  type>::type>::type* = nullptr>
  FunctionView(F&& f)
      : call_(CallVoidPtr<typename std::remove_reference<F>::type>) {
    f_.void_ptr = &f;
  }

  template <typename F,
            typename std::enable_if<std::is_function<
                typename std::remove_pointer<
                    typename std::remove_reference<F>::type>::type>::value>::
                type* = nullptr>
  FunctionView(F&& f)
      : call_(f ? CallFunPtr<typename std::remove_pointer<F>::type> : nullptr) {
    f_.fun_ptr = reinterpret_cast<void (*)()>(f);
  }

  template <typename F,
            typename std::enable_if<std::is_same<
                std::nullptr_t,
                typename std::remove_cv<F>::type>::value>::type* = nullptr>
  FunctionView(F&& /* f */) : call_(nullptr) {}

  FunctionView() : call_(nullptr) {}

  RetT operator()(ArgT... args) const {
    return call_(f_, std::forward<ArgT>(args)...);
  }

  explicit operator bool() const { return !!call_; }

 private:
  union VoidUnion {
    void* void_ptr;
    void (*fun_ptr)();
  };

  template <typename F>
  static RetT CallVoidPtr(VoidUnion vu, ArgT... args) {
    return (*static_cast<F*>(vu.void_ptr))(std::forward<ArgT>(args)...);
  }
  template <typename F>
  static RetT CallFunPtr(VoidUnion vu, ArgT... args) {
    return (reinterpret_cast<typename std::add_pointer<F>::type>(vu.fun_ptr))(
        std::forward<ArgT>(args)...);
  }

  VoidUnion f_;
  RetT (*call_)(VoidUnion, ArgT...);
};

}  // namespace webrtc

#endif  // API_FUNCTION_VIEW_H_
