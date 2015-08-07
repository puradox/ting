#pragma once

#ifdef __linux__
    #include <alsa/asoundlib.h>
    #include "plat/linux/common.hpp"
#elif __APPLE__
    #error "Platform not yet implemented"
#elif _WIN32
    #error "Platform not yet implemented"
#else
    #error "Unknown platform"
#endif
