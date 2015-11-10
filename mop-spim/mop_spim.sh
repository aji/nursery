#!/bin/sh

if ! [ -e a.out -a -x a.out ]; then
  echo "a.out does not exist or is not executable"
fi

if [ -z "$1" ]; then
  echo "usage: $0 PROGRAM"
fi

if ! [ -e "$1" ]; then
  echo "specified input file does not exist!"
fi

F=$1.s
./a.out < "$1" > "$F" && spim -file "$F"
