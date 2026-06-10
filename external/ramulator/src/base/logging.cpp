#include "base/logging.h"


namespace Ramulator {

Logger_t Logging::create_logger(std::string name, std::string pattern) {
  std::string full_name = "Ramulator::" + name;

  // Return existing logger if already registered (can happen when
  // libramulator.so is loaded by multiple shared libraries in the
  // same process, each with its own copy of the inline static init).
  auto existing = spdlog::get(full_name);
  if (existing) {
    return existing;
  }

  auto logger = spdlog::stdout_color_st(full_name);

  if (!logger) {
    throw InitializationError("Error creating logger {}!", name);
  }

  logger->set_pattern(pattern);
  logger->set_level(spdlog::level::debug);
  return logger;
}

Logger_t Logging::get(std::string name) {
  auto logger = spdlog::get("Ramulator::" + name);
  if (logger) {
    return logger;
  } else {
    throw std::runtime_error(
      fmt::format(
        "Logger {} does not exist!",
        name
      )
    );
  }
}

bool Logging::_create_base_logger() {
  auto logger = create_logger("Base");
  if (logger) {
    return true;
  } else {
    throw InitializationError("Error creating the base logger!");
  }
  return false;
}

}        // namespace Ramulator
