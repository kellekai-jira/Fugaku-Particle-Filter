import subprocess
from melissa_da_study import *

run_melissa_da_study(
        executable='simulation2-pdaf',
        total_steps=18,
        ensemble_size=9,
        assimilator_type=ASSIMILATOR_PDAF,
        cluster_name='local',
        procs_server=3,
        procs_runner=2,
        n_runners=3,
        show_server_log = True,
        show_simulation_log = True)

exit(subprocess.call(["bash", "test.sh"]))
