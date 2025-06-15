echo_n() {
  printf '%s' "$1"
}

test() {
  set -e
  name=$1
  in=$2
  out=$3
  if [ $debug = 1 ]
  then
    echo_n "$in"
    exit 0
  fi
  mkdir -p tmp
  dir="t/tmp/${0##*/} $name"
  mkdir -p "$dir"
  echo_n "$in" | ./forth > "$dir/actual"
  echo_n "$out" > "$dir/expected"
  diff "$dir/expected" "$dir/actual" || echo_n 'not '
  echo "ok 1 - $0 $name"
  echo '1..1'
}

debug=0
if [ x$1 = x-d ]
then
  debug=1
fi
