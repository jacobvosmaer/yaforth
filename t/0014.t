#!/bin/sh
. ./t/test.sh
test 'unknown word' '
foobar
' '  error: unknown word
  token: foobar
'
