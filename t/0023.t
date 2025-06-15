#!/bin/sh
. ./t/test.sh
test 'or and' '
0 0 or .
1 0 or .
0 1 or .
1 1 or .
.s
0 0 and .
1 0 and .
0 1 and .
1 1 and .
.s
' ' 0 1 1 1<0> 0 0 0 1<0>'
