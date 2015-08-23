#!/bin/sh -e

for i in {1..3}
do
  echo
  g++ -DNDEBUG -O$i aperft.cpp -o aperft.O$i
  du -b "aperft.O$i"
  ./aperft.O$i > /dev/null || :
  time ./aperft.O$i epd perftsuite.epd $1 | tail -2
done
echo

