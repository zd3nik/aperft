#!/bin/bash -e

PROG="${1-aperft}"
for i in {1..3}
do
  echo
  g++ -DNDEBUG -O$i "$PROG.cpp" -o "$PROG.O$i"
  du -b "$PROG.O$i"
  "./$PROG.O$i" > /dev/null || :
  time "./$PROG.O$i" epd perftsuite.epd $2 | tail -2
done
echo

