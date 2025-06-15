#!/bin/sh
. ./t/test.sh
test 'immediate' ': x immediate 1 ;
: y x ;
.
' ' 1'
