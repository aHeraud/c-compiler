#include "target.h"

#include <string.h>


const target_t TARGET_X86_64_UNKNOWN_LINUX_GNU = {
    .name = "x86_64-unknown-linux-gnu",
    .arch = "x86_64",
    .vendor = "unknown",
    .os = "linux-gnu",
};

const target_t* SUPPORTED_TARGETS[] = {
    &TARGET_X86_64_UNKNOWN_LINUX_GNU,
};

const char *get_native_target(void) {
#if (defined(__x86_64__) || defined(_M_AMD64) ) && defined(__gnu_linux__)
    return TARGET_X86_64_UNKNOWN_LINUX_GNU.name;
#elif (defined(__x86_64__) || defined(_M_AMD64) ) && (defined(_WIN32) || defined(_WIN64))
    return "x86_64-pc-windows-msvc";
#endif
    return NULL;
}

const target_t *get_target(const char *triple) {
    for (int i = 0; i < sizeof(SUPPORTED_TARGETS) / sizeof(target_t*); i += 1) {
        const target_t *target = SUPPORTED_TARGETS[i];
        if (strcmp(triple, target->name) == 0) return target;
    }
    return NULL;
}
