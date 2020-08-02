export SONAR_TOKEN=$SONAR_TOKEN_esp32_wifi_manager
if [ "$SONAR_TOKEN" = "" ]; then
     echo Environment variable "SONAR_TOKEN_esp32_wifi_manager" is not set
     exit 1
fi

USAGE="Usage: `basename "$0"` {prep | scan [--cache_disabled]}"

PROJECT_KEY=ruuvi_esp32-wifi-manager
BW_OUTPUT=bw-output-$PROJECT_KEY
PROJECT_VERSION=0.0
BUILD=../../build

case $1 in 
    prep)
        mkdir -p $BUILD
        cd $BUILD
        ninja -t clean
        build-wrapper-linux-x86-64 --out-dir $BW_OUTPUT ninja -j $(nproc) ruuvi_gateway_esp.elf
        ;;
    scan)
        [ ! -d "$BUILD/$BW_OUTPUT" ] && echo "Directory $BUILD/$BW_OUTPUT DOES NOT exists, run 'prep'" && exit 1
        if [ "$2" == "" ]; then
            CACHE_ENABLED=""
        elif [ "$2" == "--cache_disabled" ]; then
            CACHE_ENABLED="-Dsonar.cfamily.cache.enabled=false"
        else
            echo $USAGE
            exit 1
        fi
        sonar-scanner \
          -Dsonar.organization=ruuvi \
          -Dsonar.projectKey=$PROJECT_KEY \
          -Dsonar.sources=. \
          -Dsonar.cfamily.build-wrapper-output=$BUILD/$BW_OUTPUT \
          -Dsonar.host.url=https://sonarcloud.io \
          -Dsonar.cfamily.threads=$(nproc) \
          -Dsonar.projectVersion=$PROJECT_VERSION \
          $CACHE_ENABLED
        ;;
    *)
        echo $USAGE
        exit 1
        ;;
esac

