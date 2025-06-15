#/bin/sh
. ./t/test.sh
test '2 stack' '
1 2 2dup .s
clr
1 2 3 4 2swap .s
' '<4> 1 2 1 2<4> 3 4 1 2'
