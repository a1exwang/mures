#pragma once

#include <JuceHeader.h>
#include "../loudmon/filter_ui.h"

class LoudnessMonitor :public juce::Component {
 public:
  LoudnessMonitor();

  void updateUI() {
    std::unique_lock _(lock_);
    label_.setText(display_, NotificationType::dontSendNotification);
  }

  void prepareToPlay(double sample_rate);
  void processBlock(const AudioBuffer<float>& buffer);

 private:
  float freq_split_lowmid = 200, freq_split_midhigh = 2000;
  float q = 0.1f;

  dsp::IIR::Filter<float> low_filter, high_filter;
  PeakFilter<float> mid_filter;
  std::mutex lock_;
  std::stringstream text;
  std::string display_;
  Label label_;
};