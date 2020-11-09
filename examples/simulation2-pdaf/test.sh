#!/bin/bash

cd STATS

echo test with verification from standard model

verification_path=$PDAF_PATH/tutorial/verification/online_2D_parallelmodel

verification_path=$PDAF_PATH/tutorial/online_2D_parallelmodel


echo sometimes it is useful to rebuild the reference files
echo "(especially after an PDAF update)"
echo for this type rm $verification_path/ens_06_step08_ana.txt

# To create the testset
if [ ! -f "$verification_path/ens_06_step08_ana.txt" ]; then
  set -e
  cd $verification_path
  PDAF_ARCH=linux_gfortran_openmpi make model_pdaf
  mpirun -np 18 ./model_pdaf -dim_ens 9
  cd -
  set +e
fi



check="python2 $verification_path/../../testsuite/tests_dummy1D/check.py"

cd $verification_path
cd -

rm -f failed.log

function my_diff {
  python "$MELISSA_DA_SOURCE_PATH/examples/simulation2-pdaf/diff.py" "$fn1" "$verification_path/$fn2"
  res=$?
  if [ "$res" != "0" ];
  then
    echo ERROR! not identical: $fn1 $verification_path/$fn2 >> failed.log
    exit 1
  fi
}

for stepi in `seq 1 9`;
do
  step=`printf '%02d' $((stepi*2))`
  echo .
  echo .
  for ens in `seq 1 9`;
  do
    for typ in ana for;
    do
      fn1="ens_0${ens}_step${step}_$typ.txt"
      echo $fn1:
      ens2=$ens
      fn2="ens_0${ens2}_step${step}_$typ.txt"
      my_diff $fn1 $verification_path/$fn2  &
    done
  done
done

wait

if [[ -f "failed.log" ]];
then
  failed=`cat failed.log | wc -l`
else
  failed=0
fi
echo .
echo .
echo ===================================
if [ "$failed" == "0" ];
then
  echo passed!
else
  echo FAILED! ERROR!
  echo $failed tests failed!
  echo see failed.log!
  exit 1
fi
