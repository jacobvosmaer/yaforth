#!/bin/sh
. ./t/test.sh
test 'add' '1 2 + .
' ' 3'
