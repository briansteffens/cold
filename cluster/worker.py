#!/usr/bin/env python3

import sys
import os
import errno
import subprocess
import shutil
import fcntl
from time import sleep

import requests
import json

SERVER_URL = 'http://localhost:5000/'
TOKEN = 'secrets!'
WORKER_ID = sys.argv[1]
CORES = 6
WORKING_DIR = 'workers/' + WORKER_ID + '/'
SOLVER_FILE = WORKING_DIR + 'solver.solve'

first_status = True
assembly_queue = []

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

shutil.rmtree('workers')
mkdir_p(WORKING_DIR)

processes = []

def launch_process(assembly):
    global processes

    cmd = ('bin/cold solve {} --assembly={} --assembly-count=1 '
           '--non-interactive --all --output-dir={} --hide-solutions') \
            .format(SOLVER_FILE, assembly, WORKING_DIR)

    p = {
        'assembly': assembly,
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
        'assemblies_queued': assembly_queue,
        'assemblies_completed': [],
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

        if p['process'].poll() == None:
            continue

        data['assemblies_completed'].append({
            'assembly': p['assembly'],
            'programs_completed': p['programs_completed'],
        })

        to_remove.append(p)

    processes = [p for p in processes if p not in to_remove]

    # Set assemblies running state
    data['assemblies_running'] = [{
        'assembly': p['assembly'],
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

        with open(SOLVER_FILE, 'w') as f:
            f.write(res['solver'])

    # Load new assemblies into the queue if the server sent any
    if 'next_assemblies' in res:
        assembly_queue.extend(res['next_assemblies'])

    # Launch processes off the queue up to max cores
    while len(assembly_queue) > 0 and len(processes) < CORES:
        n = assembly_queue.pop(0)
        launch_process(n)

    sleep(30 if res['status'] == 'paused' else 1)
