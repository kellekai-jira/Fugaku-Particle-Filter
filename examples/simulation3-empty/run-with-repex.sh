#!/bin/bash


source ../../build/install/bin/melissa-da_set_env.sh

# to be changed by the user:

export EXPERIMENT_NAME=Simulation3-Empty-localnode

# can be used for hyperparameters...
export FOLDER_POSTFIX=""

# space seperated list. if one path is a folder all it's content is copied.
# absolute and relative paths are allowed.
export INPUT_FILES="runall.sh run.sh run-juwels.sh"

# git repositories to save:
# absolute and relative paths to the git repositories root dir are allowed.
# a git diff and the checked out git revision hash will get stored.
export GIT_REPOS="$MELISSA_DA_PATH/../../../melissa-da"
# ^^ trick the basename so we will retrieve the correct name for the git repo files...


function run {
  # script that runs your experiment goes here
  # it can make use of command line parameters! as $1...N
  ./runall.sh
}

# don't forget to export  your function
export -f run

# if this line fails install the git repo to your path in .bashrc!!
source experiment.sh
