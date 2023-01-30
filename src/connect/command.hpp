#pragma once

#include "bigbuffer.hpp"

#include <cstdint>
#include <string_view>
#include <variant>

namespace connect_client {

using CommandId = uint32_t;

struct UnknownCommand {};
struct BrokenCommand {
    const char *reason = nullptr;
};
struct ProcessingOtherCommand {};
struct ProcessingThisCommand {};
struct GcodeTooLarge {};
struct Gcode {
    // The actual GCode is stored in the BigBuffer, we have just the size here.
    //
    // Stored without the 0 at the end.
    size_t size;
    SharedResetToken buffer;
};
struct SendInfo {};
struct SendJobInfo {
    uint16_t job_id;
};
struct SendFileInfo {
    // The actual path is stored in the BigBuffer as LongPathBuffer
    SharedResetToken buffer;
};
struct SendTransferInfo {};
struct PausePrint {};
struct ResumePrint {};
struct StopPrint {};
struct StartPrint {
    // The actual path is stored in the BigBuffer as LongPathBuffer
    SharedResetToken buffer;
};
struct SetPrinterReady {};
struct CancelPrinterReady {};
struct StartConnectDownload {
    uint64_t team;
    // The rest is stored inside the BigBuffer as DownloadData
    SharedResetToken buffer;
};

using CommandData = std::variant<UnknownCommand, BrokenCommand, GcodeTooLarge, ProcessingOtherCommand, ProcessingThisCommand, Gcode, SendInfo, SendJobInfo, SendFileInfo, SendTransferInfo, PausePrint, ResumePrint, StopPrint, StartPrint, SetPrinterReady, CancelPrinterReady, StartConnectDownload>;

struct Command {
    CommandId id;
    CommandData command_data;
    // Note: Might be a "Broken" command or something like that. In both cases.
    static Command gcode_command(CommandId id, const std::string_view &body, BufferResetToken buff);
    // The buffer is either used and embedded inside the returned command or destroyed, resetting the buffer.
    static Command parse_json_command(CommandId id, const std::string_view &body, BufferResetToken buff);
};

}
