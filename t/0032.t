#!/bin/sh
. ./t/test.sh
test 'mod' '
5 1 mod .
5 2 mod .
5 3 mod .
5 4 mod .
5 5 mod .
5 6 mod .
.s cr' ' 0 1 2 1 0 5<0>
'
