#pragma once

// The path length constants live in gui :-(
#include <gui/file_list_defs.h>

#include <array>
#include <memory>
#include <variant>
#include <cstdint>

namespace connect_client {

using PathBuffer = std::array<char, FILE_PATH_BUFFER_LEN>;
using NameBuffer = std::array<char, FILE_NAME_BUFFER_LEN>;

// Used for the info from marlin to Connect.
struct MarlinPaths {
    PathBuffer path;
    NameBuffer name;
    MarlinPaths();
};

using LongPathBuffer = std::array<char, FILE_PATH_BUFFER_LEN + FILE_NAME_BUFFER_LEN>;

struct DownloadData {
    // The hash (file-ID) is up to 28 chars long string.
    static constexpr size_t HashSize = 29;
    std::array<char, HashSize> hash;
};

struct CommandDetails {
    LongPathBuffer path;
    std::variant<std::monostate, DownloadData> additional;
};

// Could we negotiate something smaller?
constexpr size_t GcodeMaxLen = 512;
using GcodeBuffer = std::array<uint8_t, GcodeMaxLen>;

// A buffer for storing larger stuff during the lifetime of connect.
//
// We abuse the fact that either connect wants to send some longer info to us
// (eg. paths or gcode), or we want to send some info from marlin to the
// server, but not both at the same time.
//
// Therefore, we use the same memory area for both and take turns as needed.
//
// A command (parsed, from server to us) is able to use this in case it is
// monostate (meaning empty) or paths from marlin which we won't use ‒ we are
// allowed to get a command only in case we are not processing something else.
//
// A command keeps it there until it is destroyed. We use the BufferResetToken
// for the delayed destruction.
using BigBuffer = std::variant<
    // Buffer currently unused.
    std::monostate,
    // PathAndName is used for reporting stuff from Marlin to the server
    MarlinPaths,
    // Details about a parsed command.
    CommandDetails,
    GcodeBuffer>;

// A RAII helper to eventually reset the buffer back to "empty".
class BufferResetToken {
private:
    BigBuffer *buff;

public:
    BufferResetToken(BigBuffer *buffer)
        : buff(buffer) {}
    ~BufferResetToken() {
        if (buff != nullptr) {
            *buff = std::monostate {};
        }
    }
    BufferResetToken(const BufferResetToken &other) = delete;
    BufferResetToken(BufferResetToken &&other)
        : buff(other.buff) {
        other.buff = nullptr;
    }
    BufferResetToken &operator=(const BufferResetToken &other) = delete;
    BufferResetToken &operator=(BufferResetToken &&other) {
        buff = other.buff;
        other.buff = nullptr;
        return *this;
    }
    BigBuffer *buffer() { return buff; }
    const BigBuffer *buffer() const { return buff; }
};

// TODO: This is a bit of a waste (the dynamic allocation + thread synchronization on the ref count).
//
// We could just put a ref count into the BigBuffer itself instead... More work, but better for the CPU.
using SharedResetToken = std::shared_ptr<BufferResetToken>;

}
