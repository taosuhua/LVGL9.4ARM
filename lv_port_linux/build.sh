#!/bin/bash
function cue() {
    echo
    echo "Usage:"
    echo "  ./build.sh -x86 "
    echo "  ./build.sh -x86-rebuild "
    echo "  ./build.sh -arm64 "
    echo "  ./build.sh -arm64-rebuild "
    echo "  ./build.sh -clean  "
    echo
}

while test $# -gt 0
do
  case "$1" in
  -x86)
    platform="x86"
    ;; 
  -arm64)
    platform="arm64"
    ;; 
  -x86-rebuild)
    platform="x86rebuild"
    ;; 
  -arm64-rebuild)
    platform="arm64rebuild"
    ;; 
  -clean)
    rm ./buildarm/ -rf
    rm ./buildx86/ -rf
    echo "clean project success"
    exit 0
    ;;
  -h)
    cue
    exit 0
    ;;
  *)
  esac
  shift
done

# if [ ! -d ./build ]; then
#     mkdir build
# fi

if [ -z ${platform} ]; 
then
    cue
    exit 0
fi

if [ ${platform} == "x86" ];
then
    echo "build linux app"
    rm -rf buildx86
    mkdir -p buildx86
    cd buildx86
    cmake -DCONFIG=sdl -DCMAKE_TOOLCHAIN_FILE="buildx86.cmake" ..
    make -j"$(nproc)"
    exit 0
fi

if [ ${platform} == "x86rebuild" ];
then
    echo "rebuild linux app"
    cd buildx86
    make -j"$(nproc)"
    exit 0
fi

if [ ${platform} == "arm64" ];
then
    export STAGING_DIR="${toolchain_path}":$STAGING_DIR
    echo "build rk3566 app"
    rm -rf buildarm
    mkdir -p buildarm
    cd buildarm
    cmake -DCONFIG=fbdev -DCMAKE_TOOLCHAIN_FILE="buildarm.cmake" ..
    make -j"$(nproc)"
    exit 0
fi

if [ ${platform} == "arm64rebuild" ];
then
    export STAGING_DIR="${toolchain_path}":$STAGING_DIR
    echo "rebuild rk3566 app"
    cd buildarm
    make -j"$(nproc)"
    exit 0
fi