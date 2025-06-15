#!/bin/sh
. ./t/test.sh
test 'if then else' '
: testelse if 1 . else 2 . then cr ;
1 testelse 0 testelse
' ' 1
 2
'
