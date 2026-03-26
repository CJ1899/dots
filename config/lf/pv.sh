#!/usr/bin/env bash
file="$1"
w="$2"
h="$3"
x="$4"
y="$5"

if [[ "$(file -Lb --mime-type "$file")" =~ ^image/ ]]; then
    # -s points to the socket we defined in the wrapper
    ueberzugpp cmd -s "$UB_SOCKET" -a add -i "preview" -x "$x" -y "$y" -w "$w" -h "$h" -f "$file"
    exit 1
fi

cat "$file"
