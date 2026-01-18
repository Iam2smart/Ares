#pragma once

#include <ares/types.h>

namespace ares {
namespace osd {

class MenuSystem {
public:
    MenuSystem();
    Result initialize();
    void handleInput(int key);

private:
    bool m_initialized = false;
};

} // namespace osd
} // namespace ares
