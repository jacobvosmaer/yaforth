#!/bin/sh
. ./t/test.sh
test '."' '." hello world!

"
."  x
"
: HELLO ." HELLO WORLD!" cr ;
HELLO
.s cr
' 'hello world!

 x
HELLO WORLD!
<0>
'
