#!/bin/sh
. ./t/test.sh
test 'rot' '1 2 3 rot .s' '<3> 2 3 1'
