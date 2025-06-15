#!/bin/sh
. ./t/test.sh
test '\ comments' '." hello" \ ignored
32 emit
." world! \ not ignored
"
: a\b 123 . cr ;
a\b' 'hello world! \ not ignored
 123
'
