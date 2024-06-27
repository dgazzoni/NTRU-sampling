set(SOURCES_UNITY cmov.c kem.c neon_poly_mod.c owcpa.c pack3.c packq.c poly_r2_inv.c poly.c)
set(SOURCES_NO_UNITY neon_poly_lift.c poly_s3_inv.c)

set(SOURCES_NTRU_OPT neon_batch_multiplication.c neon_matrix_transpose.c neon_poly_rq_mul.c)

# This is used to avoid a multiply-defined symbol linking error in macOS in the test executable, which links both the
# sorting and shuffling version of the libraries. Unclear why the error only happens when using unity builds.
set(DUPLICATE_SYMBOLS
    ${FIPS202_DUPLICATE_SYMBOLS}
    schoolbook_neon schoolbook_half_8x_neon transpose_8x16 transpose_8x32 half_transpose_8x16 half_transpose_8x32
    poly_neon_reduction poly_mul_neon tc3_evaluate_neon_SB1 tc3_evaluate_neon_combine neon_toom_cook_333_combine
    tc3_interpolate_neon_SB1 tc3_interpolate_neon_SB2 tc3_interpolate_neon_SB3
    karat_neon_evaluate_SB0 karat_neon_interpolate_SB0)

if(APPLE)
    set(SOURCES_NTRU_AMX amx_poly_rq_mul.c)
    set(IMPLS neon amx)
else()
    set(IMPLS neon)
endif()

set(SOURCES_hps2048509 neon_sample_iid.c)
set(SOURCES_hps2048677 sample_iid.c)
set(SOURCES_hps4096821 sample_iid.c)
set(SOURCES_hrss701 sample_iid.c)

set(SPEED_PREFIXES speed)
set(SPEED_SOURCES speed.c)
set(SPEED_NTESTSS 1024)

foreach(PARAMETER_SET KAT_NUM IN ZIP_LISTS PARAMETER_SETS KAT_NUMS)
    set(SAMPLINGS sorting)

    if(NOT PARAMETER_SET STREQUAL hrss701)
        list(APPEND SAMPLINGS shuffling)
    endif()

    foreach(IMPL ${IMPLS})
        if(IMPL STREQUAL amx)
            set(ALLOC mmap)
        else()
            set(ALLOC stack)
        endif()

        if(NOT PARAMETER_SET STREQUAL hrss701)
            list(APPEND OPT_HPS_IMPLS ntru${PARAMETER_SET}_NG21_${IMPL})
            set(OPT_HPS_IMPLS ${OPT_HPS_IMPLS} PARENT_SCOPE)
        endif()

        foreach(SAMPLING ${SAMPLINGS})
            if(PARAMETER_SET STREQUAL hrss701)
                set(LIBRARY ntru${PARAMETER_SET}_NG21_${IMPL})
            else()
                set(LIBRARY ntru${PARAMETER_SET}_NG21_${IMPL}_${SAMPLING})
            endif()

            set(PQCGENKAT_KEM PQCgenKAT_kem_${LIBRARY})

            add_library(${LIBRARY} STATIC ${HASH_SOURCES})
            target_compile_options(${LIBRARY} PUBLIC -DCRYPTO_NAMESPACE\(s\)=${LIBRARY}_\#\#s)
            target_link_libraries(${LIBRARY} PUBLIC neon_rng)

            if(IMPL STREQUAL neon)
                set(SOURCES_IMPL ${SOURCES_NTRU_OPT})
            else()
                set(SOURCES_IMPL ${SOURCES_NTRU_AMX})
                target_sources(${LIBRARY} PRIVATE ${AMX_SOURCES})
                target_include_directories(${LIBRARY} PRIVATE ${AMX_PATH})
            endif()

            if(CMAKE_UNITY_BUILD)
                set_target_properties(${LIBRARY} PROPERTIES UNITY_BUILD_MODE GROUP)
            endif()

            foreach(SOURCE ${SOURCES_UNITY} ${SOURCES_NO_UNITY} ${SOURCES_IMPL} ${SOURCES_${PARAMETER_SET}})
                set(SOURCE_FULL_PATH ${ALLOC}/neon-${PARAMETER_SET}/${SOURCE})
                target_sources(${LIBRARY} PRIVATE ${SOURCE_FULL_PATH})

                if(CMAKE_UNITY_BUILD AND NOT SOURCE IN_LIST SOURCES_NO_UNITY)
                    set_source_files_properties(${SOURCE_FULL_PATH} PROPERTIES UNITY_GROUP "${LIBRARY}")
                endif()
            endforeach()

            foreach(DUPLICATE_SYMBOL ${DUPLICATE_SYMBOLS})
                target_compile_definitions(${LIBRARY} PRIVATE ${DUPLICATE_SYMBOL}=${LIBRARY}_${DUPLICATE_SYMBOL})
            endforeach()

            target_include_directories(${LIBRARY} PUBLIC
                ${ALLOC}/neon-${PARAMETER_SET} ${HASH_PATH} ${SORT_PATH} ${RAND_PATH})

            foreach(SPEED_PREFIX SPEED_SOURCE SPEED_NTESTS IN ZIP_LISTS SPEED_PREFIXES SPEED_SOURCES SPEED_NTESTSS)
                set(SPEED ${SPEED_PREFIX}_${LIBRARY})

                add_executable_with_symlink(${SPEED} ${ALLOC}/neon-${PARAMETER_SET}/${SPEED_SOURCE})
                target_compile_definitions(${SPEED} PRIVATE NTESTS=${SPEED_NTESTS})

                target_link_libraries(${SPEED} PRIVATE ${LIBRARY} neon_rng cycles)
            endforeach()

            add_executable_with_symlink(${PQCGENKAT_KEM}
                ${CMAKE_SOURCE_DIR}/reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/PQCgenKAT_kem.c)
            target_compile_options(${PQCGENKAT_KEM} PRIVATE -Wno-unused-result)
            target_link_libraries(${PQCGENKAT_KEM} PRIVATE ${LIBRARY})

            ADD_KAT_TESTS(${KAT_TYPE})

            if(PARAMETER_SET STREQUAL hrss701)
                target_sources(${LIBRARY} PRIVATE ${SORT_SOURCES} ${ALLOC}/neon-${PARAMETER_SET}/sample.c)
            else()
                if(SAMPLING STREQUAL "sorting")
                    target_sources(${LIBRARY} PRIVATE ${SORT_SOURCES} ${ALLOC}/neon-${PARAMETER_SET}/sample.c)
                else()
                    target_sources(${LIBRARY} PRIVATE
                        ${CMAKE_SOURCE_DIR}/shuffling/opt_neon/ntru${PARAMETER_SET}/sample.c)
                    target_compile_definitions(${LIBRARY} PUBLIC SHUFFLING)
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()
