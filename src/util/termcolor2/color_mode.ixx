module;

// external
#include <spdlog/details/os.h>
#include <spdlog/spdlog.h>

export module termcolor2.color_mode;

export namespace termcolor2::details {

class ColorMode {
public:
  inline void set(spdlog::color_mode mode) {
    switch (mode) {
      case spdlog::color_mode::always:
        should_color_ = true;
        return;
      case spdlog::color_mode::automatic:
        should_color_ = spdlog::details::os::is_color_terminal();
        return;
      case spdlog::color_mode::never:
        should_color_ = false;
        return;
      default:
        __builtin_unreachable();
    }
  }
  [[nodiscard]] inline auto should_color() const -> bool {
    return should_color_;
  }

  static auto instance() -> ColorMode& {
    static ColorMode INSTANCE;
    return INSTANCE;
  }

private:
  // default: automatic
  bool should_color_ = spdlog::details::os::is_color_terminal();
};

} // namespace termcolor2::details

export namespace termcolor2 {

inline void set_color_mode(spdlog::color_mode cm) {
  details::ColorMode::instance().set(cm);
}

inline auto should_color() -> bool {
  return details::ColorMode::instance().should_color();
}

} // namespace termcolor2
