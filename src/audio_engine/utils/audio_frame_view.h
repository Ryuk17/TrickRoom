/*
 * Minimal AudioFrameView for the pure build.
 * Extracted from modules/audio_processing/include/audio_frame_view.h.
 */
#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_FRAME_VIEW_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_FRAME_VIEW_H_

#include "utils/audio_view.h"

namespace webrtc {

template <class T>
class AudioFrameView {
 public:
  AudioFrameView(T* const* audio_samples, int num_channels, int channel_size)
      : view_(num_channels && channel_size ? audio_samples : nullptr,
              channel_size,
              num_channels) {}

  template <class U>
  AudioFrameView(AudioFrameView<U> other) : view_(other.view()) {}

  template <class U>
  explicit AudioFrameView(DeinterleavedView<U> view) : view_(view) {}

  AudioFrameView() = delete;

  int num_channels() const { return view_.num_channels(); }
  int samples_per_channel() const { return view_.samples_per_channel(); }
  MonoView<T> channel(int idx) { return view_[idx]; }
  MonoView<const T> channel(int idx) const { return view_[idx]; }
  MonoView<T> operator[](int idx) { return view_[idx]; }
  MonoView<const T> operator[](int idx) const { return view_[idx]; }

  DeinterleavedView<T> view() { return view_; }
  DeinterleavedView<const T> view() const { return view_; }

 private:
  DeinterleavedView<T> view_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_FRAME_VIEW_H_
