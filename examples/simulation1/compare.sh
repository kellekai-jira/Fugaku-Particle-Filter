#!/bin/bash

echo
echo
echo
echo '***** Comparing: ******'
diff -s --side-by-side output.txt $1
res=$?
echo .
echo .
if [[ "$res" == "0" ]];
then
  echo PASSED!
  exit 0
else
  echo ERROR!
  exit 1
fi
