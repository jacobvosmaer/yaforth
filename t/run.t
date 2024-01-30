#!/bin/sh
set -e
cd t
ls -d [0-9]* | awk 'END {print "1.." NR}'
i=1
for t in $(ls -d [0-9]* | sort); do
  (../forth < $t/in > actual && diff $t/out actual) || printf 'not '
  echo "ok $i - $t/: $(cat $t/desc)"
  i=$(($i + 1))
done
