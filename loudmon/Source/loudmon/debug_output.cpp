#include "debug_output.h"

#include <iostream>
static DebugOutputComponent* the_main_logger = nullptr;
void log(int level, const std::string &s) {
  if (the_main_logger) {
    std::stringstream ss;

    std::string time_str(29, '0');
    char* buf = &time_str[0];
    auto point = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(point);
    std::strftime(buf, 21, "%Y-%m-%dT%H:%M:%S.", std::localtime(&now_c));
#ifdef JUCE_MSVC
    sprintf(buf + 20, "%09lld", point.time_since_epoch().count() % 1000000000);
#else
    sprintf(buf + 20, "%09ld", point.time_since_epoch().count() % 1000000000);
#endif
    ss << "LOG(" << std::setw(2) << std::setfill('0') << level << ") " << time_str << " " << s;
    the_main_logger->print_line(ss.str());
  }
}

DebugOutputComponent::DebugOutputComponent() :mono_font(juce::Font::getDefaultMonospacedFontName(), 20, Font::plain) {
  if (the_main_logger == nullptr) {
    the_main_logger = this;
  }
  setSize(400, 200);
  startTimer(50);
}
DebugOutputComponent::~DebugOutputComponent() {
  the_main_logger = nullptr;
}
void DebugOutputComponent::print_line(const std::string &s) {
  std::unique_lock<std::mutex> _(lines_lock);
  lines.push_back(s);
  if (lines.size() > max_line_count) {
    lines.pop_front();
  }
}
void DebugOutputComponent::timerCallback() {
  repaint();
}
void DebugOutputComponent::paint(Graphics &g) {
  std::stringstream ss;
  {
    std::unique_lock<std::mutex> _(lines_lock);
    ss << "output: " << std::endl;
    for (auto &line : lines) {
      ss << line << std::endl;
    }
  }

  g.fillAll(Colours::black);
  g.setFont(mono_font);
  g.setColour(juce::Colour::fromRGB(248, 248, 248));
  g.drawMultiLineText(ss.str(), 10, 10, getWidth() - 20);
}
