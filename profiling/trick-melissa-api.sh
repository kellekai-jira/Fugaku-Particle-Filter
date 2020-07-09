#!/usr/bin/bash
cd $HOME/workspace/melissa-da/build/install/lib
mv libmelissa_api.so libmelissa_api.so.profilling
ln -s ../../../build-no-profiling/install/lib/libmelissa_api.so .
