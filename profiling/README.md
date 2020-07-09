to run with profiling:
```
export MELISSA_PROFILING=TRUE
./compile.sh
cd profiling
```
... and execute either `python3 trace.py` or python3 `tests_slurm.py`


seems like prepost step takes long....
shorten it..

To only profile the server nd not the api you should build without profiling, move your build dir to
`build-no-profiling` and then execute `./trick-melissa-api.sh`

This will link the melissa_api to the api without profiling.
