#!/bin/sh
. ./t/test.sh
test 'if-then' ': 1? 1 = if 1 . then 2 . ;
0 1?
1 1?
' ' 2 1 2'
