\ This startup file gets interpreted before forth starts
\ reading from stdin.

: rot >r swap r> swap ;
: over >r dup r> swap ;
: cr 10 emit ;
: if immediate ' branch0 , here 0 , ;
: then immediate dup here swap - swap ! ;
: not 0 = ;
: <> = not ;
: and not not swap not not * ;
: or not swap not and not ;
: begin immediate here ;
: again immediate ' branch , here - , ;
: while immediate ' branch0 , here 0 , ;
: repeat immediate ' branch , swap here - , dup here swap - swap ! ;
: ."
  begin key dup 32 = while drop repeat emit
  begin key dup dup 34 <> swap -1 > and while emit repeat drop
;
