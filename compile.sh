#!/bin/bash

# 컴파일러
CC=gcc

# 소스 파일 및 실행 파일 이름
SOURCE="bandit_bot.c"
OUTPUT="bandit_bot_sh"

# 컴파일 및 링크 (libssh 라이브러리 포함)
echo "Compiling ${SOURCE}..."
$CC -Wall -o $OUTPUT $SOURCE -lssh
echo "Done. Run with ./${OUTPUT}"