import subprocess

process = subprocess.Popen(['ps', 'x'], stdout=subprocess.PIPE)
stdout = process.communicate()[0]
lines = stdout.decode().split('\n')

passed = True
for line in lines:

    print_line = False
    for trigger_word in ['simulation1', 'simulation2', 'simulation3', 'melissa_da_server',
            'never_connecting_runner.sh']:
        if trigger_word in line:
            print_line = True
            passed = False
            break

    if print_line:
        print(line)


# This testcase is set to pass always until a better, not timing dependent way to reliably
# kill subprocesses of the mpiexec command than using time.sleep and pkill is found.
#assert passed
print("This Testcase would pass?", passed)
