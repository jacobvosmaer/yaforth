#!/bin/sh
. ./t/test.sh
test 'compile error' '
: fac dup 1 > if dup 1 - fac * then ;
: fac recursive dup 1 > if dup 1 - fac * then ;
3 fac .
' '  error: unknown word
  token: fac
 6'
