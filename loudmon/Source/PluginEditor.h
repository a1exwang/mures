#pragma once

#include <list>
#include <string>
#include <map>
#include <utility>

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "loudmon/filter_ui.h"
#include "loudmon/oscilloscope.h"
#include "synth/synth.h"
#include "common/ui_updater.h"
#include "display/loudness.h"

class DebugOutputWindow;
class MainInfo {
 public:
  explicit MainInfo(std::function<void()> update_callback) : update_callback_(std::move(update_callback)) { }
  void set_sample_rate(double sample_rate) {
    sample_rate_ = sample_rate;
    update_callback_();
  }
  void set_samples_per_block(size_t value) {
    samples_per_block_ = value;
    update_callback_();
  }
  void set_fps(float fps) {
    fps_ = fps;
    update_callback_();
  }
  void set_input_rms(std::vector<float> values) {
    input_rms_ = std::move(values);
    update_callback_();
  }
  void set_latency(float ms, float max_expected, size_t late) {
    latency_ms_ = ms;
    latency_max_expected_ = max_expected;
    late_block_count_ = late;
    update_callback_();
  }
  void set_process_block_interval(float seconds) {
    process_block_interval_ = seconds;
    update_callback_();
  }
  void set_input_channels(size_t n) {
    input_channels_ = n;
    update_callback_();
  }
  void set_entropy(double entropy) {
    entropy_ = entropy;
    update_callback_();
  }

  void add_display_value(const std::string& key, float value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << value;
    display_values_[key] = ss.str();
  }
  void add_display_value(const std::string& key, std::string value) {
    if (display_values_.find(key) == display_values_.end()) {
      display_value_keys_in_order_.push_back(key);
    }
    display_values_[key] = std::move(value);
  }

  [[nodiscard]]
  std::string to_string() const;
 private:
  friend class MainComponent;
  double sample_rate_ = 44100;
  size_t samples_per_block_ = 2048;
  size_t input_channels_ = 2;
  float fps_ = 0;
  std::vector<float> input_rms_;
  float latency_ms_ = 0, latency_max_expected_ = 0, process_block_interval_ = 0;
  size_t late_block_count_ = 0;
  double entropy_ = 0;

  std::map<std::string, std::string> display_values_;
  std::list<std::string> display_value_keys_in_order_;
  std::function<void()> update_callback_;
};

class MainComponent : public AudioProcessorEditor, public UIUpdater, public juce::MenuBarModel {
 public:
  explicit MainComponent(NewProjectAudioProcessor& p);
  MainComponent(NewProjectAudioProcessor& p, double sample_rate, size_t block_size, size_t input_channels) :MainComponent(p) {
    prepare_to_play(sample_rate, block_size, input_channels);
  }
  ~MainComponent() override;

  StringArray getMenuBarNames() override;
  PopupMenu getMenuForIndex(int topLevelMenuIndex, const String &menuName) override;
  void menuItemSelected (int menu_item_id, int top_level_menu_index) override;

  void toggle_debug_window();
  /* Component callbacks, UI thread */
  void visibilityChanged() override;
  void paint(Graphics& g) override;
  void resized() override {
    resize_children();
  }

  /* May be in any thread */
  void set_input_rms(const std::vector<float>& values) {
    enqueue_ui([this, values]() {
      main_info_.set_input_rms(values);
      repaint();
    });
  }
  void set_latency_ms(float ms, float max_expected, size_t late) {
    enqueue_ui([this, ms, max_expected, late]() {
      main_info_.set_latency(ms, max_expected, late);
      repaint();
    });
  }
  void set_process_block_interval(float seconds) {
    enqueue_ui([this, seconds]() {
      main_info_.set_process_block_interval(seconds);
      repaint();
    });
  }
  void repaint_safe() {
    enqueue_ui([this]() {
      std::stringstream ss;
      ss << main_info_.to_string();
      ss << "Queue: UI " << std::fixed << std::setprecision(1) << get_average_ui_latency()*1000 << "ms" << "/" << get_ui_queue_size() << "/" << get_ui_queue_loss() << std::endl <<
         "UI proc: " << get_average_ui_processing_latency()*1000 << "ms" << std::endl;
      info_text.setText(ss.str(), dontSendNotification);
      repaint();
    });
  }

  void prepare_to_play(double sample_rate, size_t samples_per_block, size_t input_channels);

  SynthControl &get_synth_control() {
    return synth_control_;
  }
  void calculate_spectrum();
  void calculate_entropy();
  void reset_entropy();
  void send_block(float sample_rate, AudioBuffer<float> buffer);

  void add_display_value(const std::string& key, std::string value) {
    enqueue_ui([this, key, value{std::move(value)}]() {
      main_info_.add_display_value(key, value);
      repaint();
    });
  }
  void add_display_value(const std::string& key, float value) {
    enqueue_ui([this, key, value]() {
      main_info_.add_display_value(key, value);
      repaint();
    });
  }

  MidiBuffer get_keyboard_midi_buffer(size_t sample_count) {
    MidiBuffer ret;
    // this is thread safe, so we don't need a lock
    keyboard_state_.processNextMidiBuffer(ret, 0, static_cast<int>(sample_count), true);
    return ret;
  }
//
//  SynthControl &get_synth_control() {
//    return synth_control_;
//  }

 protected:
  void resize_children();

 private:
  MainInfo main_info_;

  LoudnessMonitor loudness_monitor_;

  std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::function<void()>>>>> menu_items_;
  juce::MenuBarComponent menu_bar_;

  Label info_text;

  Component::SafePointer<DebugOutputWindow> debug_window;
  bool debug_window_visible_ = false;

  std::chrono::high_resolution_clock::time_point last_paint_time;
  AudioBuffer<float> buffer_;

  const size_t entropy_bits = 16;
  std::vector<size_t> value_counts_ = std::vector<size_t>(size_t(int(1 << entropy_bits)), size_t(0));

  juce::MidiKeyboardState keyboard_state_;
  juce::MidiKeyboardComponent keyboard_;

  SynthControl synth_control_;
  PlotComponent debug_plot;
  //==============================================================================
  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

