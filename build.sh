#!/bin/sh
# One source (ida.S), two builds that differ only in the renderer.
#   ida_gui.s : RENDER=1, LED animation, open in the Ripes GUI
#   ida_cli.s : RENDER=0, renderer compiled out, measure with --iret
set -e
clang -E -P -x assembler-with-cpp -DRENDER=1 ida.S -o ida_gui.s
clang -E -P -x assembler-with-cpp -DRENDER=0 ida.S -o ida_cli.s
echo "built ida_gui.s and ida_cli.s"