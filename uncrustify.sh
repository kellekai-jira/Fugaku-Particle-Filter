#!/bin/bash
find . -name "*.cxx" ! -path './examples/parflow-pure/parflow-melissa-da/*' -exec uncrustify -c ./uncrustify.cfg --no-backup {} \;
find . -name "*.h" ! -path './examples/parflow-pure/parflow-melissa-da/*' -exec uncrustify -c ./uncrustify.cfg --no-backup {} \;
