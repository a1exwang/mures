#include "loudness.h"

template <typename T>
T calculate_rms(const T *data, size_t size) {
  double sum = 0;
  for (size_t i = 0; i < size; i++) {
    sum += data[i] * data[i];
  }
  sum /= size;
  std::stringstream ss;
  if (sum > 0) {
    return static_cast<T>(10.0 * log10(sum));
  }
  else {
    return std::numeric_limits<T>::min();
  }
}

void LoudnessMonitor::processBlock(const AudioBuffer<float> &buffer) {
  std::stringstream mid_rms, low_rms, high_rms;
  AudioBuffer<float> merge_buffer(1, buffer.getNumSamples());
  for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
    merge_buffer.addFrom(0, 0, buffer, channel, 0, buffer.getNumSamples(), 1.0/buffer.getNumChannels());
  }


//  for (int channel = 0; channel < synth_channels; channel++) {
//    if (editor->is_main_filter_enabled()) {
//      AudioBuffer<float> audio_buffer_main(1, getBlockSize());
//      audio_buffer_main.copyFrom(0, 0, buffer, channel, 0, getBlockSize());
//      dsp::AudioBlock<float> audio_block_main(audio_buffer_main);
//      dsp::ProcessContextReplacing<float> replacing_context_main(audio_block_main);
//      editor->filter_process(channel, replacing_context_main);
//      buffer.copyFrom(channel, 0, audio_buffer_main, 0, 0, getBlockSize());
//    }
  auto rms = calculate_rms(buffer.getArrayOfReadPointers()[0], buffer.getNumSamples());
  text << std::fixed << std::setprecision(2) << rms << "/";

  AudioBuffer<float> audio_buffer_filter(1, buffer.getNumSamples());
  dsp::AudioBlock<float> audio_block_filter(audio_buffer_filter);
  dsp::ProcessContextReplacing<float> replacing_context(audio_block_filter);

  audio_buffer_filter.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
  low_filter.process(replacing_context);
  auto channel_rms = calculate_rms(audio_buffer_filter.getArrayOfReadPointers()[0], buffer.getNumSamples());
  text << std::fixed << std::setprecision(2) << channel_rms << "/";

  audio_buffer_filter.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
  mid_filter.process(replacing_context);
  channel_rms = calculate_rms(audio_buffer_filter.getArrayOfReadPointers()[0], buffer.getNumSamples());
  text << std::fixed << std::setprecision(2) << channel_rms << "/";

  audio_buffer_filter.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
  high_filter.process(replacing_context);
  channel_rms = calculate_rms(audio_buffer_filter.getArrayOfReadPointers()[0], buffer.getNumSamples());
  text << std::fixed << std::setprecision(2) << channel_rms << " dB RMS";
//  }

  {
    std::unique_lock _(lock_);
    display_ = text.str();
  }
}

void LoudnessMonitor::prepareToPlay(double sample_rate) {
  low_filter.coefficients = dsp::IIR::Coefficients<float>::makeLowPass(sample_rate, freq_split_lowmid, q);
  mid_filter.get<0>() = dsp::IIR::Coefficients<float>::makeLowPass(sample_rate, freq_split_midhigh, q);
  mid_filter.get<1>() = dsp::IIR::Coefficients<float>::makeHighPass(sample_rate, freq_split_lowmid, q);
  high_filter.coefficients = dsp::IIR::Coefficients<float>::makeLowPass(sample_rate, freq_split_midhigh, q);
}
LoudnessMonitor::LoudnessMonitor() {
  addAndMakeVisible(label_);
  label_.setText("123", dontSendNotification);
  label_.setBounds(getBounds());
}
