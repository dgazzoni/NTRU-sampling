include(CheckCSourceRuns)

if(APPLE)
    set(ARM_ARCHITECTURE_NAME arm64)
else()
    set(ARM_ARCHITECTURE_NAME aarch64)
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL ${ARM_ARCHITECTURE_NAME})
    set(CMAKE_REQUIRED_FLAGS -march=armv8-a+crypto)
    check_c_source_runs("
#include <arm_neon.h>

int main() {
    uint8x16_t t = vdupq_n_u8(0);

    t = vaesmcq_u8(t);

    return 0;
}
" HAVE_CRYPTO_EXTENSIONS)
else()
    option(HAVE_CRYPTO_EXTENSIONS "builds with ARMv8 crypto extensions" OFF)
endif()
