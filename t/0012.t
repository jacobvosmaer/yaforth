#!/bin/sh
. ./t/test.sh
test 'factorial' ': fac recursive dup 1 > if dup 1 - fac * then ;
3 fac .
' ' 6'
