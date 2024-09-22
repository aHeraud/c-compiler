#ifndef C_COMPILER_TARGET_H
#define C_COMPILER_TARGET_H

#include "ir/arch.h"

typedef enum ArchType {
    X86_64,
} arch_type_t;

typedef struct Arch {
    const char* name;
    arch_type_t arch_type;
    const ir_arch_t *ir_arch;
} arch_t;

typedef struct Target {
    const char *name;
    const arch_t *arch;
    const char *vendor;
    const char *os;
} target_t;

/**
 * Get the native/host target (which the compiler was built for).
 * @return Native target triple, or NULL if not detectable or un-supported.
 */
const char *get_native_target(void);

/**
 * Get the target corresponding to the supplied target tripple.
 * @param triple target tripple (e.g. x86_64-unknown-linux-gnu) to search for
 * @return Target (if one exists), or NULL
 */
const target_t *get_target(const char *triple);

extern const arch_t ARCH_X86_64;
extern const target_t TARGET_X86_64_UNKNOWN_LINUX_GNU;

extern const target_t* SUPPORTED_TARGETS[];

#endif
