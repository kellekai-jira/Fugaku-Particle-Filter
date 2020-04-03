#!/usr/bin/python2
##!/home/friese/.local/bin/ipython -i

# inspired by testsuite/tests_dummy1D/check.py

import numpy as np
import sys
a = np.genfromtxt(sys.argv[1])
b = np.genfromtxt(sys.argv[2])
limit = 1.e-8
diff = np.max(np.abs(a - b))
if diff > limit:
    print('Max difference', diff)
    print('  this bypasses the limit of', limit)
    print('shapes', a.shape, b.shape)
    concat = np.concatenate((a,b))
    print('Data range', np.min(concat), np.max(concat))
    print('Data mean is 0 in some filters', np.mean(concat))
    exit(1)
else:
    exit(0)

