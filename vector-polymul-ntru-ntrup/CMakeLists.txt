set(SOURCES_UNITY cmov.c kem.c owcpa.c pack3.c packq.c poly_lift.c poly_mod.c poly_r2_inv.c poly.c)
set(SOURCES_NO_UNITY poly_s3_inv.c sample_iid.c)

set(SOURCES_hps2048677_tc batch_multiplication.c tc.c)
set(SOURCES_hps2048677_tmvp batch_multiplication.c tmvp.c)

set(SOURCES_hrss701_tmvp batch_multiplication.c tmvp2.c)

set(DUPLICATE_SYMBOLS
    ${FIPS202_DUPLICATE_SYMBOLS}
    poly_mul_neon tc33_mul schoolbook_8x8 schoolbook_16x16 itc5 tc5 itc33 tc33 ik2 k2 tmvp33_last tmvp tmvp2_8x8
    ittc5 ttc5 ittc3 tmvp33 ttc33 ittc32)

if(APPLE)
    set(SOURCES_hps2048677_amx amx_poly_rq_mul.c)
    set(SOURCES_hrss701_amx amx_poly_rq_mul.c)
    set(IMPLS_hps2048677 amx tc tmvp)
    set(IMPLS_hrss701 amx tmvp)
else()
    set(IMPLS_hps2048677 tc tmvp)
    set(IMPLS_hrss701 tmvp)
endif()

set(KAT_NUMS_CCHY23 1234 1450)
set(PARAMETER_SETS hps2048677 hrss701)

set(SPEED_PREFIXES speed)
set(SPEED_SOURCES speed.c)
set(SPEED_NTESTSS 1024)

foreach(PARAMETER_SET KAT_NUM IN ZIP_LISTS PARAMETER_SETS KAT_NUMS_CCHY23)
    set(SAMPLINGS sorting)

    if(NOT PARAMETER_SET STREQUAL hrss701)
        list(APPEND SAMPLINGS shuffling)
    endif()

    foreach(IMPL ${IMPLS_${PARAMETER_SET}})
        if(IMPL STREQUAL amx)
            set(ALLOC mmap)
        else()
            set(ALLOC stack)
        endif()

        if(NOT PARAMETER_SET STREQUAL hrss701)
            list(APPEND OPT_HPS_IMPLS ntru${PARAMETER_SET}_CCHY23_${IMPL})
            set(OPT_HPS_IMPLS ${OPT_HPS_IMPLS} PARENT_SCOPE)
        endif()

        foreach(SAMPLING ${SAMPLINGS})
            if(PARAMETER_SET STREQUAL hrss701)
                set(LIBRARY ntru${PARAMETER_SET}_CCHY23_${IMPL})
            else()
                set(LIBRARY ntru${PARAMETER_SET}_CCHY23_${IMPL}_${SAMPLING})
            endif()

            set(PQCGENKAT_KEM PQCgenKAT_kem_${LIBRARY})

            add_library(${LIBRARY} STATIC ${HASH_SOURCES})
            target_compile_options(${LIBRARY} PUBLIC -DCRYPTO_NAMESPACE\(s\)=${LIBRARY}_\#\#s)
            target_link_libraries(${LIBRARY} PUBLIC neon_rng)

            if(IMPL STREQUAL amx)
                target_sources(${LIBRARY} PRIVATE ${AMX_SOURCES})
                target_include_directories(${LIBRARY} PRIVATE ${AMX_PATH})
            endif()

            if(CMAKE_UNITY_BUILD)
                set_target_properties(${LIBRARY} PROPERTIES UNITY_BUILD_MODE GROUP)
            endif()

            foreach(SOURCE ${SOURCES_UNITY} ${SOURCES_NO_UNITY} ${SOURCES_${PARAMETER_SET}_${IMPL}})
                set(SOURCE_FULL_PATH ntru${PARAMETER_SET}/${ALLOC}/aarch64_${IMPL}/${SOURCE})
                target_sources(${LIBRARY} PRIVATE ${SOURCE_FULL_PATH})

                if(CMAKE_UNITY_BUILD AND NOT SOURCE IN_LIST SOURCES_NO_UNITY)
                    set_source_files_properties(${SOURCE_FULL_PATH} PROPERTIES UNITY_GROUP "${LIBRARY}")
                endif()
            endforeach()

            foreach(DUPLICATE_SYMBOL ${DUPLICATE_SYMBOLS})
                target_compile_definitions(${LIBRARY} PRIVATE ${DUPLICATE_SYMBOL}=${LIBRARY}_${DUPLICATE_SYMBOL})
            endforeach()

            target_include_directories(${LIBRARY} PUBLIC
                ntru${PARAMETER_SET}/${ALLOC}/aarch64_${IMPL} ${HASH_PATH} ${SORT_PATH} ${RAND_PATH})

            foreach(SPEED_PREFIX SPEED_SOURCE SPEED_NTESTS IN ZIP_LISTS SPEED_PREFIXES SPEED_SOURCES SPEED_NTESTSS)
                set(SPEED ${SPEED_PREFIX}_${LIBRARY})

                add_executable_with_symlink(${SPEED} ntru${PARAMETER_SET}/${ALLOC}/aarch64_${IMPL}/${SPEED_SOURCE})
                target_compile_definitions(${SPEED} PRIVATE NTESTS=${SPEED_NTESTS})

                target_link_libraries(${SPEED} PRIVATE ${LIBRARY} neon_rng cycles)
            endforeach()

            add_executable_with_symlink(${PQCGENKAT_KEM}
                ${CMAKE_SOURCE_DIR}/reference/Reference_Implementation/crypto_kem/ntru${PARAMETER_SET}/PQCgenKAT_kem.c)
            target_compile_options(${PQCGENKAT_KEM} PRIVATE -Wno-unused-result)
            target_link_libraries(${PQCGENKAT_KEM} PRIVATE ${LIBRARY})

            ADD_KAT_TESTS(${KAT_TYPE})

            if(PARAMETER_SET STREQUAL hrss701)
                target_sources(${LIBRARY} PRIVATE
                    ${SORT_SOURCES} ntru${PARAMETER_SET}/${ALLOC}/aarch64_${IMPL}/sample.c)
            else()
                if(SAMPLING STREQUAL "sorting")
                    target_sources(${LIBRARY} PRIVATE
                        ${SORT_SOURCES} ntru${PARAMETER_SET}/${ALLOC}/aarch64_${IMPL}/sample.c)
                else()
                    target_sources(${LIBRARY} PRIVATE
                        ${CMAKE_SOURCE_DIR}/shuffling/opt_neon/ntru${PARAMETER_SET}/sample.c)
                    target_compile_definitions(${LIBRARY} PUBLIC SHUFFLING)
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()
