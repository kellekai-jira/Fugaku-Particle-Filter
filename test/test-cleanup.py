import subprocess

process = subprocess.Popen(['ps', 'x'], stdout=subprocess.PIPE)
stdout = process.communicate()[0]
lines = stdout.decode().split('\n')

passed = True
for line in lines:

    print_line = False
    for trigger_word in ['simulation1', 'simulation2', 'simulation3', 'melissa_server',
            'never_connecting_runner.sh']:
        if trigger_word in line:
            print_line = True
            passed = False
            break

    if print_line:
        print(line)


assert passed
