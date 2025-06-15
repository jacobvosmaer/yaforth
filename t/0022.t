#!/bin/sh
. ./t/test.sh
test 'not

' '1 not .
0 not .
2 not .
-1 not .
' ' 0 1 0 0'
