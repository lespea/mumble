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
build:
    {{ tset }} cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_UNITY_BUILD=ON \
        -DCMAKE_CXX_FLAGS="-gz=zstd -fno-plt" \
        -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold -gz=zstd -Wl,--no-as-needed -lmimalloc" \
        -Dclient=ON \
        -Dserver=OFF \
        -Doverlay=OFF \
        -Doverlay-xcompile=OFF \
        -Ddbus=ON \
        -Dlto=ON \
        -Dplugins=OFF \
        -Dsymbols=ON \
        -Dupdate=OFF \
        -Doptimize=ON

    {{ tset }} mold -run ninja -C build

# Alias for build
alias release := build

# Clean build directory
clean:
    rm -rf build
