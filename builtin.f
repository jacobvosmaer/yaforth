\ This startup file gets interpreted before forth starts
\ reading from stdin.

: if immediate ' branch0 , here 0 , ;
: else immediate ' branch , here 0 , swap dup here swap - swap ! ;
: then immediate dup here swap - swap ! ;

: begin immediate here ;
: again immediate ' branch , here - , ;
: while immediate ' branch0 , here 0 , ;
: repeat immediate ' branch , swap here - , dup here swap - swap ! ;

: rot >r swap r> swap ;
: over >r dup r> swap ;

: not 0 = ;
: <> = not ;
: and not not swap not not * ;
: or not swap not and not ;

: cr 10 emit ;
: ."
  key drop \ skip exactly 1 blank after first quote
  begin key dup dup 34 <> swap -1 > and while emit repeat drop
;
