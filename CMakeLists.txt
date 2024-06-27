cmake_minimum_required(VERSION 3.18)

project(ntru-sampling C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

option(CMAKE_UNITY_BUILD "merge source files into batches for faster compilation" ON)
option(SANITIZER "build with AddressSanitizer and UBSanitizer support" OFF)
option(BUILD_TESTING "build with tests enabled" ON)
option(USE_FEAT_DIT "enable device-independent timing bit" OFF)

set(CMAKE_UNITY_BUILD_BATCH_SIZE 0)

if(BUILD_TESTING)
    include(CTest)
endif()

include(BuildCaching.cmake)
include(CreateSymlink.cmake)
include(CPUCore.cmake)
include(HaveCryptoExtensions.cmake)

add_compile_options(-Wall -Wextra -pedantic)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-language-extension-token -Wno-strict-prototypes)
endif()

if((CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 15) OR
    CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wno-array-parameter)
endif()

if(SANITIZER)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

if(HAVE_CRYPTO_EXTENSIONS)
    add_compile_options(-march=armv8-a+crypto)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        "SHELL:-mllvm -align-all-functions=6" "SHELL:-mllvm -align-all-nofallthru-blocks=6")
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-falign-functions=64)
endif()

if(APPLE)
    execute_process(COMMAND brew --prefix openssl OUTPUT_VARIABLE OPENSSL_ROOT_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

find_package(OpenSSL REQUIRED)

set(BUILD_GMOCK OFF CACHE BOOL "")
set(INSTALL_GTEST OFF CACHE BOOL "")
add_subdirectory(googletest)

include(GoogleTest)
set(GTEST_DISCOVERY_TIMEOUT 60)

add_library(ref_rng OBJECT reference/Reference_Implementation/crypto_kem/ntruhps2048509/rng.c)
target_compile_definitions(ref_rng PUBLIC randombytes_init=nist_randombytes_init randombytes=nist_randombytes)
target_compile_options(ref_rng PRIVATE -Wno-sign-compare -Wno-unused-parameter)

if((CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 10) OR
    CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(ref_rng PRIVATE -Wno-unused-but-set-variable)
endif()

target_link_libraries(ref_rng PUBLIC OpenSSL::SSL OpenSSL::Crypto)

if(HAVE_CRYPTO_EXTENSIONS)
    message(STATUS "Using accelerated AES RNG")

    set(KAT_TYPE aes)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_library(opt_rng OBJECT rng_opt/rng_inline_asm.c)
    else()
        add_library(opt_rng OBJECT rng_opt/rng.c)
        set_source_files_properties(rng_opt/rng.c PROPERTIES COMPILE_FLAGS -fno-strict-aliasing)
    endif()

    target_compile_definitions(opt_rng PUBLIC randombytes_init=opt_randombytes_init randombytes=opt_randombytes)
    add_library(neon_rng ALIAS opt_rng)

    add_executable(test_rng test/test_rng.cpp)
    target_include_directories(test_rng PRIVATE reference/Reference_Implementation/crypto_kem/ntruhps2048509)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(test_rng PRIVATE -Wno-macro-redefined)
    endif()

    target_link_libraries(test_rng PRIVATE ref_rng opt_rng gtest_main)
    gtest_discover_tests(test_rng DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})

    add_executable(speed_rng speed/speed_rng.c)
    target_link_libraries(speed_rng PRIVATE ref_rng neon_rng cycles)
else()
    message(STATUS "Using ChaCha20 RNG")
    set(KAT_TYPE chacha20)

    add_library(chacha20_rng OBJECT
        vector-polymul-ntru-ntrup/randombytes/chacha20.c vector-polymul-ntru-ntrup/randombytes/randombytes.c
        vector-polymul-ntru-ntrup/randombytes/rng.c)
    target_compile_definitions(chacha20_rng PUBLIC
        randombytes_init=chacha20_randombytes_init randombytes=chacha20_randombytes NORAND)

    add_library(neon_rng ALIAS chacha20_rng)
endif()

set(REF_COMMON_SOURCES
    cmov.c fips202.c kem.c owcpa.c pack3.c packq.c poly.c poly_lift.c
    poly_mod.c poly_r2_inv.c poly_s3_inv.c poly_rq_mul.c sample_iid.c)

set(REF_SORTING_SOURCES crypto_sort_int32.c)
set(REF_SAMPLING_SOURCES sample.c)

set(HPS_PARAMETER_SETS hps2048509 hps2048677 hps4096821)
set(PARAMETER_SETS ${HPS_PARAMETER_SETS} hrss701)
set(KAT_NUMS 935 1234 1590 1450)

macro(ADD_KAT_TESTS KAT_TYPE)
    # https://stackoverflow.com/a/3071370/523079
    add_test(
        NAME ${LIBRARY}.KATs_match_spec
        COMMAND
        ${CMAKE_COMMAND}
        -DKATgen_cmd=${PQCGENKAT_KEM}
        -DKAT_expected=${CMAKE_SOURCE_DIR}/KAT/${SAMPLING}/${KAT_TYPE}/ntru${PARAMETER_SET}/PQCkemKAT_${KAT_NUM}
        -DKAT_actual=PQCkemKAT_${KAT_NUM}
        -DWORKING_DIRECTORY=${CMAKE_BINARY_DIR}/KAT/${LIBRARY}
        -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
        -DSRC_DIRECTORY=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/CompareKATs.cmake)
endmacro()

foreach(PARAMETER_SET KAT_NUM IN ZIP_LISTS PARAMETER_SETS KAT_NUMS)
    set(SAMPLINGS sorting)

    if(NOT PARAMETER_SET STREQUAL hrss701)
        list(APPEND SAMPLINGS shuffling)
    endif()

    foreach(SAMPLING ${SAMPLINGS})
        if(PARAMETER_SET STREQUAL hrss701)
            set(LIBRARY ref_ntru${PARAMETER_SET})
        else()
            set(LIBRARY ref_ntru${PARAMETER_SET}_${SAMPLING})
        endif()

        set(PQCGENKAT_KEM PQCgenKAT_kem_${LIBRARY})

        add_library(${LIBRARY} STATIC)
        target_include_directories(${LIBRARY} PUBLIC
            reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET})

        target_compile_options(${LIBRARY} PUBLIC -DCRYPTO_NAMESPACE\(s\)=ntru_\#\#s)

        if(CMAKE_C_COMPILER_ID MATCHES "GNU")
            target_compile_options(${LIBRARY} PRIVATE -Wno-stringop-overread)
        endif()

        target_link_libraries(${LIBRARY} PUBLIC ref_rng OpenSSL::SSL OpenSSL::Crypto)

        add_executable(${PQCGENKAT_KEM} reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/PQCgenKAT_kem.c)
        target_compile_options(${PQCGENKAT_KEM} PRIVATE -Wno-unused-result)
        target_link_libraries(${PQCGENKAT_KEM} PRIVATE ${LIBRARY})

        if(CMAKE_UNITY_BUILD)
            set_target_properties(${LIBRARY} PROPERTIES UNITY_BUILD_MODE GROUP)
            set_target_properties(${PQCGENKAT_KEM} PROPERTIES UNITY_BUILD_MODE GROUP)
        endif()

        foreach(REF_COMMON_SOURCE ${REF_COMMON_SOURCES})
            target_sources(${LIBRARY} PRIVATE
                reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/${REF_COMMON_SOURCE})
        endforeach()

        ADD_KAT_TESTS(aes)
    endforeach()

    if(PARAMETER_SET STREQUAL hrss701)
        foreach(REF_SAMPLING_SOURCE ${REF_SAMPLING_SOURCES})
            target_sources(ref_ntru${PARAMETER_SET} PRIVATE
                reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/${REF_SAMPLING_SOURCE})
        endforeach()
    else()
        target_compile_definitions(ref_ntru${PARAMETER_SET}_shuffling PUBLIC SHUFFLING)

        foreach(REF_SORTING_SOURCE ${REF_SORTING_SOURCES} ${REF_SAMPLING_SOURCES})
            target_sources(ref_ntru${PARAMETER_SET}_sorting PRIVATE
                reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/${REF_SORTING_SOURCE})
        endforeach()

        foreach(REF_SHUFFLING_SOURCE ${REF_SAMPLING_SOURCES})
            target_sources(ref_ntru${PARAMETER_SET}_shuffling PRIVATE
                shuffling/ref/ntru${PARAMETER_SET}/${REF_SHUFFLING_SOURCE})
        endforeach()
    endif()
endforeach()

set(HASH_PATH ${CMAKE_SOURCE_DIR}/vector-polymul-ntru-ntrup/hash)
set(HASH_SOURCES ${HASH_PATH}/fips202.c ${HASH_PATH}/sha2.c)

set(SORT_PATH ${CMAKE_SOURCE_DIR}/vector-polymul-ntru-ntrup/sort)
set(SORT_SOURCES ${SORT_PATH}/crypto_sort.c)

set(RAND_PATH ${CMAKE_SOURCE_DIR}/rng_opt)

if(APPLE)
    set(AMX_PATH ${CMAKE_SOURCE_DIR}/amx)
    set(AMX_SOURCES ${CMAKE_SOURCE_DIR}/amx/polymodmul.c ${CMAKE_SOURCE_DIR}/amx/aux_routines.c)

    add_library(cycles OBJECT ${CMAKE_SOURCE_DIR}/vector-polymul-ntru-ntrup/cycles/m1cycles.c)
else()
    add_library(cycles OBJECT ${CMAKE_SOURCE_DIR}/vector-polymul-ntru-ntrup/cycles/hal.c)
endif()

if(USE_FEAT_DIT)
    target_sources(cycles PRIVATE ${CMAKE_SOURCE_DIR}/speed/feat_dit.c)
    target_compile_definitions(cycles PUBLIC USE_FEAT_DIT)
endif()

target_include_directories(cycles PUBLIC ${CMAKE_SOURCE_DIR}/vector-polymul-ntru-ntrup/cycles ${CMAKE_SOURCE_DIR}/speed)

set(OPT_HPS_IMPLS "")
add_subdirectory(PQC_NEON/neon/ntru)
add_subdirectory(vector-polymul-ntru-ntrup)

# Tests
foreach(OPT_HPS_IMPL ${OPT_HPS_IMPLS})
    set(TEST test_keypair_enc_dec_${OPT_HPS_IMPL})

    add_executable(${TEST} test/test_keypair_enc_dec.cpp)

    target_compile_definitions(${TEST} PRIVATE TEST_NAME=${OPT_HPS_IMPL})
    target_compile_options(${TEST} PUBLIC
        -DCRYPTO_NAMESPACE_SORTING\(s\)=${OPT_HPS_IMPL}_sorting_\#\#s
        -DCRYPTO_NAMESPACE_SHUFFLING\(s\)=${OPT_HPS_IMPL}_shuffling_\#\#s)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${TEST} PUBLIC -Wno-macro-redefined)
    endif()

    target_include_directories(${TEST} PUBLIC rng_opt)
    target_link_libraries(${TEST} PRIVATE ${OPT_HPS_IMPL}_sorting ${OPT_HPS_IMPL}_shuffling neon_rng gtest_main)

    gtest_discover_tests(${TEST} DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endforeach()

foreach(PARAMETER_SET ${HPS_PARAMETER_SETS})
    set(TEST test_sample_fixed_type_${PARAMETER_SET})

    set(REF_LIB ref_sample_fixed_type_${PARAMETER_SET})
    set(OPT_LIB opt_sample_fixed_type_${PARAMETER_SET})

    add_library(${REF_LIB} OBJECT shuffling/ref/ntru${PARAMETER_SET}/sample.c)
    target_include_directories(${REF_LIB} PUBLIC
        rng_opt reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET})
    target_compile_options(${REF_LIB} PRIVATE -DCRYPTO_NAMESPACE\(s\)=ntru_ref_shuffling_\#\#s)

    add_library(${OPT_LIB} OBJECT shuffling/opt_neon/ntru${PARAMETER_SET}/sample.c)
    target_include_directories(${OPT_LIB} PUBLIC
        rng_opt reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET})
    target_compile_options(${OPT_LIB} PRIVATE -DCRYPTO_NAMESPACE\(s\)=ntru_opt_shuffling_\#\#s)

    add_executable(${TEST} test/test_sample_fixed_type.cpp)

    target_compile_definitions(${TEST} PRIVATE TEST_NAME=sample_fixed_type_${PARAMETER_SET})
    target_include_directories(${TEST} PRIVATE
        rng_opt reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET})
    target_link_libraries(${TEST} PRIVATE ${REF_LIB} ${OPT_LIB} neon_rng gtest_main)

    gtest_discover_tests(${TEST} DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endforeach()

foreach(PARAMETER_SET ${HPS_PARAMETER_SETS})
    foreach(SAMPLING sorting shuffling)
        set(SPEED speed_sample_fixed_type_${PARAMETER_SET}_${SAMPLING})

        add_executable(${SPEED} speed/speed_sample_fixed_type.c)
        target_compile_definitions(${SPEED} PRIVATE SAMPLE_FIXED_TYPE=sample_fixed_type)
        target_include_directories(${SPEED} PUBLIC reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET})
        target_link_libraries(${SPEED} PRIVATE neon_rng cycles)

        if(SAMPLING MATCHES shuffling)
            target_compile_definitions(${SPEED} PRIVATE SHUFFLING)
        endif()
    endforeach()

    target_sources(speed_sample_fixed_type_${PARAMETER_SET}_sorting PRIVATE
        PQC_NEON/neon/ntru/stack/neon-${PARAMETER_SET}/sample.c ${SORT_SOURCES})
    target_sources(speed_sample_fixed_type_${PARAMETER_SET}_shuffling PRIVATE
        shuffling/opt_neon/ntru${PARAMETER_SET}/sample.c)
endforeach()
