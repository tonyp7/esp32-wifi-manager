#!/bin/bash

#set -x

export SONAR_TOKEN=$SONAR_TOKEN_esp32_wifi_manager
if [ "$SONAR_TOKEN" = "" ]; then
     echo Environment variable "SONAR_TOKEN_esp32_wifi_manager" is not set
     exit 1
fi

USAGE="Usage: `basename "$0"` {prep | scan [relative_or_absolute_path_to_cache_location]}"
# default cache location: /home/<user>/.sonar/cache

PROJECT_KEY=ruuvi_esp32-wifi-manager
BW_OUTPUT=bw-output-$PROJECT_KEY
ROOT_DIR=../..
SCRIPTS=$ROOT_DIR/scripts
BUILD=$ROOT_DIR/build-coverage
TESTS=tests
BUILD_TESTS=$TESTS/cmake-build-unit-tests
SOURCES=./src

CWD=$(pwd)

if [ "$PROJECT_VERSION" = "" ]; then
    PROJECT_VERSION=0.0
fi

case $1 in 
    prep)
        mkdir -p $BUILD
        cd $BUILD || exit 1
        if [ -f build.ninja ]; then
            ninja -t clean
        fi
        cmake .. -G Ninja
        build-wrapper-linux-x86-64 --out-dir $BW_OUTPUT ninja -j $(nproc) ruuvi_gateway_esp.elf
        ;;
    scan)
        [ ! -d "$BUILD/$BW_OUTPUT" ] && echo "Directory $BUILD/$BW_OUTPUT DOES NOT exists, run 'prep'" && exit 1
        if [ "$2" == "" ]; then
            CACHE_ENABLED="-Dsonar.cfamily.cache.enabled=false"
            CACHE_PATH=""
        else
            CACHE_ENABLED="sonar.cfamily.cache.enabled=true"
            CACHE_PATH="sonar.cfamily.cache.path=$2"
        fi

        mkdir -p $BUILD_TESTS
        cd $BUILD_TESTS || exit 1
        echo Initial cleanup
        find . -type f -name '*.gcna' -delete
        find . -type f -name '*.gcno' -delete
        find . -type f -name 'gtestresults.xml' -delete
        rm -f $BUILD_TESTS/test-coverage.xml

        echo Generate makefiles for unit-tests
        cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..
        test ${?} -eq 0 || exit 1

        echo Clean build
        make clean
        test ${?} -eq 0 || exit 1

        echo Build unit-tests
        make
        test ${?} -eq 0 || exit 1

        echo Run unit-tests
        ctest

        cd "$CWD" || exit 1
        test ${?} -eq 0 || exit 1

        echo Generate execution_report.xml for SonarCloud
        python3 $SCRIPTS/conv_gtestresults_to_sonarcloud.py \
            --cmake_build_path=$BUILD_TESTS \
            --output=$BUILD_TESTS/execution_report.xml \
            --update_sonar_project_properties
        test ${?} -eq 0 || exit 1

        echo Generate test-coverage.xml for SonarCloud
        gcovr . --delete --sonarqube $BUILD_TESTS/test-coverage.xml
        test ${?} -eq 0 || exit 1
        find . -type f -name '*.gcno' -delete

        echo Run sonar-scanner
        # sonar.tests is getting from sonar-project.properties
        sonar-scanner \
          -Dsonar.organization=ruuvi \
          -Dsonar.sources=$SOURCES \
          -Dsonar.host.url=https://sonarcloud.io \
          -Dsonar.coverageReportPaths=$BUILD_TESTS/test-coverage.xml \
          -Dsonar.testExecutionReportPaths=$BUILD_TESTS/execution_report.xml \
          -Dsonar.projectKey=$PROJECT_KEY \
          -Dsonar.cfamily.build-wrapper-output=$BUILD/$BW_OUTPUT \
          -Dsonar.cfamily.threads=$(nproc) \
          -Dsonar.projectVersion=$PROJECT_VERSION \
          $CACHE_ENABLED \
          $CACHE_PATH
        ;;
    *)
        echo "$USAGE"
        exit 1
        ;;
esac

