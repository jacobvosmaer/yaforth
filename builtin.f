\ This startup file gets interpreted before forth starts
\ reading from stdin.

: if immediate ' branch0 , here @ 0 , ;
: else immediate ' branch , here @ 0 , swap dup here @ swap - swap ! ;
: then immediate dup here @ swap - swap ! ;

: begin immediate here @ ;
: again immediate ' branch , here @ - , ;
: while immediate ' branch0 , here @ 0 , ;
: repeat immediate ' branch , swap here @ - , dup here @ swap - swap ! ;

: not 0 = ;
: <> = not ;
: and not not swap not not * ;
: or not swap not and not ;

: constant word create ' lit , , ' exit , ;
10 constant '\n'
34 constant '"'
41 constant ')'

: ( immediate begin key dup ')' <> swap -1 > and while repeat ;
( now we can use parentheses for comments in between program text )

: rot ( x y z -- y z x ) >r swap r> swap ;
: over ( x y -- x y x ) >r dup r> swap ;
: 2dup ( x y --- x y x y ) over over ;
: 2swap ( x y z w --- z w x y ) rot >r rot r> ;

: align here @ aligned here ! ;
: cr '\n' emit ;
: ." immediate
  state if
    ' litstring ,
    here @ 0 , \ reserve space for length
    here @ \ start of string data
    key drop \ skip leading space
    begin key dup dup '"' <> swap -1 > and while c, repeat drop
    here @ swap - swap ! \ update string length
    here @ align here ! ' tell ,
  else
    key drop \ skip exactly 1 blank after first quote
    begin key dup dup '"' <> swap -1 > and while emit repeat drop
  then
;

