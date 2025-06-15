#!/bin/sh
. ./t/test.sh
test 'state' '
: printstate immediate state . cr ;
] printstate [ printstate
' ' 1
 0
'
