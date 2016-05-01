#!/usr/bin/env python3

import sys
import os
import errno
import subprocess
import shutil
import fcntl
import glob
from time import sleep

import requests
import json

if len(sys.argv) != 5:
    print('Usage:')
    print('cluster/worker.py [cluster_server_url] [token] [worker_id] [cores]')
    sys.exit(0)

SERVER_URL = sys.argv[1]
TOKEN = sys.argv[2]
WORKER_ID = sys.argv[3]
CORES = int(sys.argv[4])

WORKING_DIR = 'workers/' + WORKER_ID + '/'
SOLVER_FILE = WORKING_DIR + 'solver.solve'

first_status = True
combination_queue = []

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

def reset_working_dir():
    global WORKING_DIR
    try:
        shutil.rmtree(WORKING_DIR)
    except:
        pass
    mkdir_p(WORKING_DIR)

reset_working_dir()

processes = []

def launch_process(combination):
    global processes

    cmd = ('bin/cold solve {} --combination={} --combination-count=1 '
           '--non-interactive --all --output-dir={} --hide-solutions') \
            .format(SOLVER_FILE, combination, WORKING_DIR)

    p = {
        'combination': combination,
        'process': subprocess.Popen(cmd.split(), stdout=subprocess.PIPE,
            bufsize=1),
        'programs_completed': 0,
    }

    # Make stdout.readline non-blocking
    flags = fcntl.fcntl(p['process'].stdout, fcntl.F_GETFL)
    fcntl.fcntl(p['process'].stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    processes.append(p)

def kill_all():
    global processes

    for p in processes:
        p['process'].kill()

    for p in processes:
        p['process'].wait()

    processes = []

while True:
    data = {
        'token': TOKEN,
        'worker_id': WORKER_ID,
        'cores': CORES,
        'combinations_queued': combination_queue,
        'combinations_completed': [],
        'solutions': [],
    }

    # Check running processes
    to_remove = []

    for p in processes:
        while True:
            line = p['process'].stdout.readline()

            if line == b'':
                break

            line = line.decode('utf-8').strip()

            if line.startswith('total: '):
                p['programs_completed'] = \
                        int(line.split(':')[1].split(',')[0].strip())

        # Check if the process is still running
        if p['process'].poll() == None:
            continue

        # Process is finished, clean up
        ac = {
            'combination': p['combination'],
            'programs_completed': p['programs_completed'],
            'solutions': [],
        }

        # Check for solutions
        solution_files = glob.glob('{}{}/solution.cold'.format(WORKING_DIR,
                str(p['combination'])))

        for solution_fn in solution_files:
            with open(solution_fn) as f:
                ac['solutions'].extend([
                        s.strip() for s in f.read().split('---') if s.strip()])

        data['combinations_completed'].append(ac)

        to_remove.append(p)

    processes = [p for p in processes if p not in to_remove]

    # Set combinations running state
    data['combinations_running'] = [{
        'combination': p['combination'],
        'programs_completed': p['programs_completed'],
    } for p in processes]

    if first_status:
        data['first_status'] = True
        first_status = False

    # Send status update to server
    try:
        r = requests.post(SERVER_URL + 'worker/status',
                headers={'Content-Type': 'application/json'},
                data=json.dumps(data))
    except:
        print('Error connecting to cluster server')
        sleep(1)
        continue

    if (r.status_code != 200):
        print('Error connecting to cluster server: ' + str(r.status_code))
        sleep(1)
        continue

    res = r.json()

    print(res)

    # Kill all processes if the server is not running
    if res['status'] != 'running':
        kill_all()

    # Write new solver file if the server sent one
    if 'solver' in res:
        kill_all()

        reset_working_dir()

        with open(SOLVER_FILE, 'w') as f:
            f.write(res['solver'])

    # Load new combinations into the queue if the server sent any
    if 'next_combinations' in res:
        combination_queue.extend(res['next_combinations'])

    # Launch processes off the queue up to max cores
    while len(combination_queue) > 0 and len(processes) < CORES:
        n = combination_queue.pop(0)
        launch_process(n)

    sleep(30 if res['status'] == 'disarmed' else 1)
