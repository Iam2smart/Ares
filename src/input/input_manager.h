#pragma once

#include <ares/types.h>
#include <string>

namespace ares {
namespace input {

class InputManager {
public:
    InputManager();
    ~InputManager();

    Result initialize(const std::string& device_path);
    Result pollInput();

private:
    bool m_initialized = false;
};

} // namespace input
} // namespace ares
