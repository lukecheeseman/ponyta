#ifndef PLATFORMFUNS_H
#define PLATFORMFUNS_H

#include <platform.h>
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// OS names used in platform.pony
#define OS_FREEBSD_NAME "freebsd"
#define OS_LINUX_NAME "linux"
#define OS_MACOSX_NAME "osx"
#define OS_WINDOWS_NAME "windows"
#define OS_POSIX_NAME "posix"
#define OS_X86_NAME "x86"
#define OS_ARM_NAME "arm"
#define OS_LP64_NAME "lp64"
#define OS_LLP64_NAME "llp64"
#define OS_ILP32_NAME "ilp32"
#define OS_NATIVE128_NAME "native128"
#define OS_DEBUG_NAME "debug"

/** Report whether the named platform attribute is true
 * @param out_is true if the specified attribute is set, false otherwise
 * @return true on success, false is specified attribute does not exist
 */
bool os_is_target(const char* attribute, bool release, bool* out_is_target, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
