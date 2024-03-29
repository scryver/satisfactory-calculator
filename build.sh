#!/bin/bash

code="$PWD"
opts="-O0 -g -ggdb -msse4 -ffast-math -Wall -Werror -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-missing-braces -Wno-unused-function"

mkdir -p gebouw

cd gebouw > /dev/null

echo Building satisfactory calc
clang++ $opts $code/src/main.cpp -o satisfactory-calc
cd $code > /dev/null
