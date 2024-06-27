set(ID_STRINGS "0xd03" "0xd07" "0xd08" "M1" "M3")
set(CORE_NAMES "Cortex-A53" "Cortex-A57" "Cortex-A72" "Apple M1" "Apple M3")
set(CORE_ABBRVS "A53" "A57" "A72" "M1" "M3")

if(NOT DEFINED CPU_CORE)
    if(APPLE)
        execute_process(
            COMMAND sysctl -n machdep.cpu.brand_string
            OUTPUT_VARIABLE SYSCTL_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(REGEX MATCH "M[1-9]" CPUINFO "${SYSCTL_OUTPUT}")
    elseif(ANDROID)
        execute_process(
            COMMAND adb ${ANDROID_SERIAL_ARGS} shell "cat /proc/cpuinfo | grep part"
            OUTPUT_VARIABLE CPUINFO)
    elseif(UNIX)
        execute_process(COMMAND bash "-c" "cat /proc/cpuinfo | grep part" OUTPUT_VARIABLE CPUINFO)
    endif()

    foreach(ID_STRING CORE_NAME CORE_ABBRV IN ZIP_LISTS ID_STRINGS CORE_NAMES CORE_ABBRVS)
        if(CPUINFO MATCHES ${ID_STRING})
            message(STATUS "CPU auto-detection found ${CORE_NAME}")
            set(CPU_CORE ${CORE_ABBRV})
            add_compile_definitions(CPU_${CPU_CORE})
            break()
        endif()
    endforeach()
endif()

if(NOT DEFINED CPU_CORE)
    message(STATUS "CPU auto-detection failed, no -mcpu=... flag will be used")
    set(CPU_CORE "GENERIC")
endif()

if(CPU_CORE STREQUAL "M1")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-mcpu=apple-m1)
    endif()
elseif(CPU_CORE STREQUAL "M3")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        # As of the time this was written, there is no -mcpu=apple-m3 flag in Clang, so go with M2
        add_compile_options(-mcpu=apple-m2)
    endif()
elseif(CPU_CORE STREQUAL "A72")
    add_compile_options(-mcpu=cortex-a72)
elseif(CPU_CORE STREQUAL "A57")
    add_compile_options(-mcpu=cortex-a57)
elseif(CPU_CORE STREQUAL "A53")
    add_compile_options(-mcpu=cortex-a53)
endif()
