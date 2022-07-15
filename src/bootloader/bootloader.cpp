#include <bootloader/bootloader.hpp>
#include "log.h"
<<<<<<< HEAD
=======
#include "bsod.h"
#include <tuple>
#include <memory>
#include "cmsis_os.h"
#include "crc32.h"
#include "bsod.h"
#include <string.h>
#include "sys.h"

#include "scratch_buffer.hpp"
#include "resources/bootstrap.hpp"
#include "resources/revision_bootloader.hpp"
#include "bootloader/bootloader.hpp"
#include "bootloader/required_version.hpp"

// FIXME: Those includes are here only for the RNG.
// We should add support for the stdlib's standard random function
#include "main.h"
#include "stm32f4xx_hal.h"
>>>>>>> Upstream/RELEASE-4.4.0

using Version = buddy::bootloader::Version;

LOG_COMPONENT_DEF(Bootloader, LOG_SEVERITY_INFO);

Version buddy::bootloader::get_version() {
    Version *const bootloader_version = (Version *const)0x0801FFFA;
    return *bootloader_version;
}
