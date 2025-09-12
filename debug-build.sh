#!/bin/bash
set -e

meson setup build --reconfigure --buildtype=debug -Dtests=true

cp build/compile_commands.json .

meson test -C build
meson install -C build
