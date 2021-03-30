## How to mesasure server coverage?

- install coverage (`pip install coverage`)

- change the server command in `script.py` into

    server_cmd='coverage run %s/server-p2p/server.py' % os.getenv('MELISSA_DA_SOURCE_PATH'),

- run it (`python3 script.py`)

- `cd STATS && coverage report -m` (coverage will create a `.coverage` file in the cwd.

