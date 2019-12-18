#!/bin/bash

#set -e
#set -x

source build/install/bin/melissa-da_set_env.sh

cd output

echo test with verification from standard model

verification_path=$PDAF_PATH/tutorial/verification/online_2D_parallelmodel

verification_path=$PDAF_PATH/tutorial/online_2D_parallelmodel

# To create the testset
if [ ! -f "$verification_path/ens_06_step08_ana.txt" ]; then
  cd $verification_path
  make model_pdaf
  mpirun -np 18 ./model_pdaf -dim_ens 9
  cd -
fi



#verification_path=/home/friese/workspace/PDAF-D_V1.14/tutorial/online_2D_parallelmodel

check="python2 $verification_path/../../testsuite/tests_dummy1D/check.py"

cd $verification_path
#mpirun -np 9 ./model_pdaf -dim_ens 9 -filtertype 6
cd -

rm -f failed.log

function my_diff {
  python ../diff.py $fn1 $verification_path/$fn2
  res=$?
  if [ "$res" != "0" ];
  then
    echo ERROR! not identical: $fn1 $verification_path/$fn2 >> failed.log
    #echo diff:
    #./diff.py $fn1 $verification_path/$fn2
    #$check $fn1 $verification_path/
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
      #for ens2 in `seq 1 3`;
      #do
        ens2=$ens
        fn2="ens_0${ens2}_step${step}_$typ.txt"
   #     echo $fn2:

        #diff -sq $fn1 $verification_path/$fn2
        #diff -q $fn1 $verification_path/$fn2
        my_diff $fn1 $verification_path/$fn2  &
      #done
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
fi
