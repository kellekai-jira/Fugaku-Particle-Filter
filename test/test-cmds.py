import subprocess

def check(cmd):
    subprocess.check_call(["/bin/bash", "-c", "which %s" % cmd])

for cmd in ['pkill', 'pgrep', 'grep', 'xargs']:
    check(cmd)
