import os
import sys
import signal
import subprocess
import random
import shutil

def killing_giraffe(name):
    p = subprocess.Popen(['ps', '-x'], stdout=subprocess.PIPE)
    out, _ = p.communicate()
    pids = []
    for line in out.splitlines():
        if ' %s' % name in line.decode():
            pids.append(int(line.split(None, 1)[0]))
    assert len(pids) > 0
    os.kill(random.choice(pids), signal.SIGKILL)

def clean_old_stats():
    print("Cleaning old results...")
    if os.path.isdir("STATS"):
        shutil.rmtree("STATS")
    else:
        print("Nothing to clean!")
