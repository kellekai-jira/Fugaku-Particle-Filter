#!/bin/bash
find . -name "*.cxx" -exec uncrustify -c ./uncrustify.cfg --no-backup {} \;
find . -name "*.h" -exec uncrustify -c ./uncrustify.cfg --no-backup {} \;
