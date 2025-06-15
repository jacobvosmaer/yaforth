#!/bin/sh
. ./t/test.sh
test '( ) comments' '
: test ( n --- ) drop ( also ignored ( no balancing ) ;
1 2 test .s cr
' '<1> 1
'
