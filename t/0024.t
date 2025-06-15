#!/bin/sh
. ./t/test.sh
test 'begin again' '
: test begin dup 0 < if exit then dup . 1 - again ;
3 test
' ' 3 2 1 0'
