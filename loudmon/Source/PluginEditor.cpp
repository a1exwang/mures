#include <chrono>
#include <algorithm>
#include <numeric>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "loudmon/debug_output.h"


std::string MainInfo::to_string() const {
  std::stringstream ss;
  ss << "FPS: " << fps_ << std::endl
     << "Latency(out/in/max/late): " << std::fixed << std::setprecision(2) << std::setfill('0')
     << process_block_interval_*1000 << "/" << latency_ms_ << "/" << latency_max_expected_ << "/" << late_block_count_ << std::endl
     << "Sample Rate: " << sample_rate_ << ", Samples per Block: " << samples_per_block_ << ", Input channels: " << input_channels_ << std::endl
     << "Input RMS: ";
  for (float value : input_rms_) {
    ss << std::fixed << std::setprecision(2) << std::setfill('0') << value << "dB ";
  }
  ss << std::endl;
  for (auto &key : display_value_keys_in_order_) {
    ss << key << ": " << display_values_.at(key) << std::endl;
  }
  ss << "Entropy: " << entropy_ << std::endl;
  return ss.str();
}

// This function is written so we can put menu implementation in cpp file
//  rather than in class definition of the header file.
static auto get_menu_items(MainComponent *that) {
  return std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::function<void()>>>>>{
      {
          "UI Scaling",
          {
              {"50%", std::bind(&MainComponent::setScaleFactor, that, 0.5f)},
              {"100%", std::bind(&MainComponent::setScaleFactor, that, 1.0f)},
              {"125%", std::bind(&MainComponent::setScaleFactor, that, 1.25f)},
              {"150%", std::bind(&MainComponent::setScaleFactor, that, 1.5f)},
              {"175%", std::bind(&MainComponent::setScaleFactor, that, 1.75f)},
              {"200%", std::bind(&MainComponent::setScaleFactor, that, 2.0f)},
              {"250%", std::bind(&MainComponent::setScaleFactor, that, 2.5f)},
          }
      },
      {
          "Debug",
          {
              {"Toggle Debug Window", std::bind(&MainComponent::toggle_debug_window, that)}
          }
      },
      {
        "Control",
            {
                {"Reset Entropy", std::bind(&MainComponent::reset_entropy, that)},
            }
      }
  };
}


MainComponent::MainComponent(NewProjectAudioProcessor& p)
    : AudioProcessorEditor(p),
      main_info_(std::bind(&MainComponent::repaint_safe, this)),
      menu_items_(get_menu_items(this)),
      menu_bar_(this),
      keyboard_(keyboard_state_, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard) {

//  debug_plot.set_value_range(0, 1, 0, 1, false, false);
//  debug_plot.add_new_values("1", {{0.1f, -0.1f}, {0.5f, 0.5f}, {0.9f, 1.1f}});
//  addAndMakeVisible(debug_plot);

  if (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::OperatingSystemType::Linux) {
    Desktop::getInstance().setGlobalScaleFactor(2);
  } else {
    Desktop::getInstance().setGlobalScaleFactor(1);
  }
  setResizable(true, true);
  setSize(800, 800);
  setResizeLimits(300, 300, std::numeric_limits<int>::max(), std::numeric_limits<int>::max());

  addAndMakeVisible(menu_bar_);

  addAndMakeVisible(keyboard_);
  addAndMakeVisible(synth_control_);

  addAndMakeVisible(info_text);
  info_text.setFont (Font (16.0f, Font::bold));
  info_text.setText("Info", NotificationType::dontSendNotification);
  info_text.setColour(Label::textColourId, Colours::orange);
  info_text.setJustificationType (Justification::left);
  info_text.setBounds(10, 10, getWidth() - 20, 400 - 20);

  addAndMakeVisible(loudness_monitor_);

  debug_window = new DebugOutputWindow("Debug Window", Colour(0), true);
  debug_window->setSize(1024, 400);
  debug_window->setResizable(true, true);

  resize_children();
}

MainComponent::~MainComponent() {
  shutdown();
  if (debug_window) {
    debug_window.deleteAndZero();
    debug_window = nullptr;
  }
}

StringArray MainComponent::getMenuBarNames() {
  StringArray names;
  for (auto &[name, _] : menu_items_) {
    names.add(name);
  }
  return names;
}

PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const String &menuName) {
  PopupMenu menu;
  (void)menuName;
  // An ID of 0 is used as a return value to indicate that the user
  // didn't pick anything, so you shouldn't use it as the ID for an item..
  int i = 1;
  for (auto &item : std::get<1>(menu_items_[topLevelMenuIndex])) {
    auto& [sub_menu_name, action] = item;
    juce::PopupMenu::Item popup_item(sub_menu_name);
    popup_item.setID(i++);
    menu.addItem(popup_item);
  }
  return menu;
}

void MainComponent::menuItemSelected(int menu_item_id, int top_level_menu_index) {
  if (top_level_menu_index >= 0 && top_level_menu_index < menu_items_.size()) {
    auto menu_item = std::get<1>(menu_items_[top_level_menu_index]);
    auto menu_item_index = menu_item_id-1;
    if (menu_item_index >= 0 && menu_item_index < menu_item.size()) {
      auto action = std::get<1>(menu_item[menu_item_index]);
      if (action) {
        action();
      }
    }
  }
}

void MainComponent::toggle_debug_window() {
  debug_window_visible_ = !debug_window_visible_;
  if (debug_window) {
    debug_window->setVisible(debug_window_visible_ && isVisible());
  }
}
void MainComponent::visibilityChanged() {
  if (debug_window) {
    debug_window->setVisible(isVisible() && debug_window_visible_);
  }
}

void MainComponent::paint(Graphics& g) {
  auto now = std::chrono::high_resolution_clock::now();
  main_info_.set_fps(1 / std::chrono::duration<float>(now-last_paint_time).count());
  last_paint_time = now;

  g.fillAll(Colour::fromRGB(0, 0, 0));
}


void MainComponent::prepare_to_play(double sample_rate, size_t samples_per_block, size_t input_channels) {
  loudness_monitor_.prepareToPlay(sample_rate);
  enqueue_ui([this, sample_rate, samples_per_block, input_channels]() {
    main_info_.set_sample_rate(sample_rate);
    main_info_.set_samples_per_block(samples_per_block);
    main_info_.set_input_channels(input_channels);
    resize_children();
  });
}


void MainComponent::resize_children() {
  auto area = getLocalBounds();
  menu_bar_.setBounds(area.removeFromTop (LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
  keyboard_.setBounds(area.removeFromBottom(50));

  auto control_area = area.removeFromLeft(static_cast<int>(area.getWidth()*(1.0f/2.0f)));
  synth_control_.setBounds(control_area);
  auto total_height = area.getHeight();
  loudness_monitor_.setBounds(area.removeFromTop(total_height/8.0));
  info_text.setBounds(area.removeFromTop(total_height/4));
}

void MainComponent::send_block(float sample_rate, AudioBuffer<float> buffer) {
  enqueue_ui([this, sample_rate, buffer{std::move(buffer)}]() {
    loudness_monitor_.processBlock(buffer);
    buffer_ = buffer;
    calculate_entropy();
  });
  enqueue_ui([this]() {
    loudness_monitor_.updateUI();
  });
}

void MainComponent::calculate_entropy() {
  enqueue_ui_processing([this]() {
    auto buf = buffer_.getReadPointer(0);
    for (auto i = 0; i < buffer_.getNumSamples(); i++) {
      auto sample_value = size_t((buf[i] + 1) / 2 * float(1ul << entropy_bits));
      if (sample_value >= value_counts_.size()) {
        sample_value = value_counts_.size() - 1;
      }
      value_counts_[sample_value]++;
    }
    size_t total_count = std::accumulate(value_counts_.begin(), value_counts_.end(), size_t(0));
    double entropy = 0;
    for (auto value_count : value_counts_) {
      double p = double(value_count) / total_count;
      if (p > 0) {
        entropy += -p * log2(p);
      }
    }
    enqueue_ui([this, entropy]() {
      main_info_.set_entropy(entropy);
    });
  });
}

void MainComponent::reset_entropy() {
  enqueue_ui_processing([this]() {
    std::fill(value_counts_.begin(), value_counts_.end(), 0);
  });
}
