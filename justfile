tset := 'taskset -ac 0-31'

# List available recipes
default:
    @just --list

# Pull latest source from git and update all submodules
pull:
    #!/usr/bin/env bash
    set -euo pipefail
    if git rev-parse --symbolic-full-name --verify -q "@{u}" > /dev/null; then
        echo "Pulling latest changes for branch '$(git branch --show-current)'..."
        git pull --recurse-submodules
    else
        echo "Branch '$(git branch --show-current)' has no tracking upstream. Fetching remotes..."
        git fetch --all --recurse-submodules=yes
    fi
    echo "Updating submodules..."
    git submodule update --init --recursive

# Configure and compile in Release profile with Ninja
config:
    {{ tset }} cmake -B build -G Ninja \
        -UCMAKE_PROJECT_TOP_LEVEL_INCLUDES \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_UNITY_BUILD=OFF \
        -DCMAKE_CXX_FLAGS="-gz=zstd -fno-plt -Wno-sfinae-incomplete" \
        -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold -gz=zstd -Wl,--as-needed -Wl,--push-state,--no-as-needed -lmimalloc -Wl,--pop-state" \
        -Dclient=ON \
        -Dserver=OFF \
        -Doverlay=OFF \
        -Doverlay-xcompile=OFF \
        -Ddbus=ON \
        -Dlto=ON \
        -Dplugins=OFF \
        -Dsymbols=ON \
        -Dupdate=OFF \
        -Doptimize=ON \
        -Dbundled-rnnoise=OFF \
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON

build:
    {{ tset }} mold -run ninja -C build

# Alias for build
alias release := build

# Clean build directory
clean:
    rm -rf build
