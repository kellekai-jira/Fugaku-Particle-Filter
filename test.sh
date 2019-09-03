 #!/bin/bash -x

echo test with verification from standard model

verification_path=/home/friese/workspace/PDAF-D_V1.14/tutorial/verification/online_2D_parallelmodel
verification_path=/home/friese/workspace/PDAF-D_V1.14/tutorial/online_2D_parallelmodel

cd $verification_path
#mpirun -np 9 ./model_pdaf -dim_ens 9 -filtertype 6
cd -

for stepi in `seq 1 9`;
do
  step=`printf '%02d' $((stepi*2))`
  echo .
  echo .
  for ens in `seq 1 3`;
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
        diff -q $fn1 $verification_path/$fn2
        res=$?
        if [ "$res" != "0" ];
        then
          echo ERROR! not identical: $fn1 and $verification_path/$fn2
          exit 1
        fi
      #done
    done
  done
done
# meld ens_03_step06_ana.txt /home/friese/workspace/PDAF-D_V1.13.2_melissa/tutorial/verification/online_2D_parallelmodel/ens_03_step06_ana.txt

echo .
echo .
echo ===================================
echo passed!
