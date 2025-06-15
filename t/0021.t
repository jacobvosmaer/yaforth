#!/bin/sh
. ./t/test.sh
test 'mem' 'here @ 4 + 123 , here @ = . cr \ comma advances here by 4
here @ 4 - @ . cr \ retrieve value 4 bytes before here -- should be 123
456 here ! here @ . cr \ store 456 at here without advancing here. fetch should yield 456
' ' 1
 123
 456
'
