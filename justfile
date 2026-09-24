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
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    ninja -C build

# Alias for build
alias release := build

# Clean build directory
clean:
    rm -rf build
