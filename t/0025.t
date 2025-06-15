#!/bin/sh
. ./t/test.sh
test 'begin while repeat

' ': test 0 >r begin r> dup >r 3 < while r> 1 + dup . >r repeat r> drop ;
test cr .s
' ' 1 2 3
<0>'
