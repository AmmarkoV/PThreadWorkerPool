#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"
  

make clean
make

valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --track-origins=yes --num-callers=20 --track-fds=yes ./example --threads 4 --iterations 1 $@ 2> dbg_log.txt
cat dbg_log.txt

exit 0
