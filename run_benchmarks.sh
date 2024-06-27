#!/bin/bash

COOLDOWN_BEGINNING=120
COOLDOWN_MIDDLE=60

if [[ "$(uname -s)" == "Darwin" ]]
then
    OS=macOS
    CPU=$(sysctl -n machdep.cpu.brand_string | sed -e 's/.*\(M[0-9]\).*/\1/')
elif [[ "$(uname -s)" == "Linux" ]]
then
    OS=Linux
    if [[ $(grep -c "part.*0xd08" /proc/cpuinfo) -gt 0 ]]
    then
        echo "Cortex-A72 detected"
        CPU=A72
    elif [[ $(grep -c "part.*0xd07" /proc/cpuinfo) -gt 0 ]]
    then
        echo "Cortex-A57 detected"
        CPU=A57
    elif [[ $(grep -c "part.*0xd03" /proc/cpuinfo) -gt 0 ]]
    then
        echo "Cortex-A53 detected"
        CPU=A53
    else
        echo "Unknown CPU detected. Aborting."
        exit 1
    fi
else
    echo "Unknown CPU detected. Aborting."
    exit 1
fi

if [[ "$OS" == "macOS" ]]
then
    # https://unix.stackexchange.com/a/389406/493812
    if [ "$(id -u)" -ne 0 ]; then
            echo 'Under macOS, this script must be run by root. Aborting.'
            exit 1
    fi
fi

# Note regarding the COMPILERS variable:
#
# Multiple compilers can be specified, if desired; each variable must be in the form
# "<location of C compiler>:<location of C++ compiler>". For example, a GCC variable may be declared in addition to
# the existing CLANG variable, and COMPILERS would be set to "$CLANG $GCC".

if [[ "$CPU" == "M1" ]]
then
    echo "Benchmarking for Apple M1"
    CLANG=/usr/bin/clang:/usr/bin/clang++
    CMAKE=/opt/homebrew/bin/cmake
    COMPILERS="$CLANG"
    FEAT_DITS="OFF ON" # Benchmark with and without DIT
elif [[ "$CPU" == "M3" ]]
then
    echo "Benchmarking for Apple M3"
    CLANG=/usr/bin/clang:/usr/bin/clang++
    CMAKE=/opt/homebrew/bin/cmake
    COMPILERS="$CLANG"
    FEAT_DITS="OFF ON" # Benchmark with and without DIT
elif [[ "$CPU" == "A72" ]]
then
    echo "Benchmarking for Cortex-A72 (Raspberry Pi 4)"
    CLANG=/usr/bin/clang-17:/usr/bin/clang++-17
    CMAKE=/usr/bin/cmake
    COMPILERS="$CLANG"
    FEAT_DITS=OFF
elif [[ "$CPU" == "A57" ]]
then
    echo "Benchmarking for Cortex-A57 (NVIDIA Jetson Nano)"
    CLANG=/usr/bin/clang-10:/usr/bin/clang++-10
    CMAKE=/snap/bin/cmake
    COMPILERS="$CLANG"
    FEAT_DITS=OFF
elif [[ "$CPU" == "A53" ]]
then
    echo "Benchmarking for Cortex-A53 (Raspberry Pi 3)"
    CLANG=/usr/bin/clang-17:/usr/bin/clang++-17
    CMAKE=/usr/bin/cmake
    COMPILERS="$CLANG"
    FEAT_DITS=OFF
fi

PATHNAME=speed_results_${CPU}

if [ -d ${PATHNAME} ]
then
    # https://stackoverflow.com/a/226724/523079
    echo "Existing results will be deleted. Are you sure?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes ) rm -rf ${PATHNAME}; break;;
            No ) exit;;
        esac
    done
fi

mkdir ${PATHNAME}

for compiler in $COMPILERS
do
    for FEAT_DIT in $FEAT_DITS
    do
        CC=$(echo $compiler | cut -d: -f1)
        CXX=$(echo $compiler | cut -d: -f2)

        echo "Compiling with $CC and $CXX"
        rm -rf build
        mkdir build
        cd build
        ${CMAKE} \
            -DCMAKE_C_COMPILER=$CC \
            -DCMAKE_CXX_COMPILER=$CXX \
            -DCMAKE_BUILD_TYPE=Release \
            -DUSE_FEAT_DIT=${FEAT_DIT} \
            -G Ninja \
            ..
        ninja

        echo Waiting for the system to cool down for ${COOLDOWN_BEGINNING} seconds...
        sleep ${COOLDOWN_BEGINNING}

        for PARAMETER_SET in hps2048509 hps2048677 hps4096821 hrss701
        do
            for SAMPLING in sorting shuffling
            do
                if [ "$PARAMETER_SET" == "hps2048677" ] || [ "$PARAMETER_SET" == "hrss701" ]
                then
                    IMPLS="CCHY23"
                else
                    IMPLS="NG21"
                fi

                for IMPL in $IMPLS
                do
                    if [ "$IMPL" == "NG21" ]
                    then
                        VARIANTS="neon"
                    else
                        VARIANTS="tmvp"
                    fi

                    if [[ "$OS" == "macOS" ]]
                    then
                        VARIANTS="amx $VARIANTS"
                    fi

                    for VARIANT in $VARIANTS
                    do
                        if [ "$PARAMETER_SET" != "hrss701" ]
                        then
                            SPEED_EXEC=speed_ntru${PARAMETER_SET}_${IMPL}_${VARIANT}_${SAMPLING}
                            OUTPUT_FILE="../${PATHNAME}/ntru:${PARAMETER_SET}:${IMPL}:${VARIANT}:${SAMPLING}:$(basename ${CC}:DIT_${FEAT_DIT}).txt"
                        else
                            # Ugly kludge to skip a second identical run
                            if [ "$SAMPLING" == "shuffling" ]
                            then
                                continue
                            fi

                            SPEED_EXEC=speed_ntru${PARAMETER_SET}_${IMPL}_${VARIANT}
                            OUTPUT_FILE="../${PATHNAME}/ntru:${PARAMETER_SET}:${IMPL}:${VARIANT}:$(basename ${CC}:DIT_${FEAT_DIT}).txt"
                        fi

                        echo "Benchmarking ${SPEED_EXEC}"
                        
                        # https://stackoverflow.com/questions/77711672/performance-of-cpu-only-code-varies-with-executable-file-name
                        cp ${SPEED_EXEC} speed

                        # Run twice to warm up; e.g. macOS needs to verify the code signature and is slower on the first
                        # run
                        ./speed > /dev/null
                        ./speed > /dev/null
                        # Actual run
                        ./speed > ${OUTPUT_FILE}

                        echo Waiting for the system to cool down for ${COOLDOWN_MIDDLE} seconds...
                        sleep ${COOLDOWN_MIDDLE}
                    done
                done

                if [ "$PARAMETER_SET" != "hrss701" ]
                then
                    SPEED_EXEC=speed_sample_fixed_type_${PARAMETER_SET}_${SAMPLING}
                    echo "Benchmarking ${SPEED_EXEC}"
                    
                    # https://stackoverflow.com/questions/77711672/performance-of-cpu-only-code-varies-with-executable-file-name
                    cp ${SPEED_EXEC} speed

                    # Run twice to warm up; e.g. macOS needs to verify the code signature and is slower on the first run
                    ./speed > /dev/null
                    ./speed > /dev/null
                    # Actual run
                    ./speed > "../${PATHNAME}/sample_fixed_type:${PARAMETER_SET}:${SAMPLING}:$(basename ${CC}:DIT_${FEAT_DIT}).txt"

                    echo Waiting for the system to cool down for ${COOLDOWN_MIDDLE} seconds...
                    sleep ${COOLDOWN_MIDDLE}
                fi
            done
        done

        cd ..
    done
done

# Only needed for macOS, but harmless on Linux
chown -R $(logname) build/ ${PATHNAME}/
