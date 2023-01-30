#pragma once

#include "bigbuffer.hpp"
#include "command.hpp"

#include <variant>

namespace connect_client {

class Printer;

struct BackgroundGcode {
    // The actual data is stored in the buffer.
    BufferResetToken buffer;
    size_t size;
    size_t position;
};

enum class BackgroundResult {
    Success,
    Failure,
    More,
    Later,
};

using BackgroundCmd = std::variant<BackgroundGcode>;

BackgroundResult step(BackgroundCmd &cmd, Printer &printer);

}
