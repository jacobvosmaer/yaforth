#!/bin/sh
. ./t/test.sh
test 'colon numbers' ': x 1 ; x . x .' ' 1 1'
