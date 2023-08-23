#!/bin/sh

set -ue

gcc -Wall -Wextra -Wpedantic -std=c11 \
    -Wno-unused-parameter \
    $(pkg-config --cflags --libs glfw3 glew cglm) -lm \
    main.c "$@"
