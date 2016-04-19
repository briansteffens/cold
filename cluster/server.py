#!/usr/bin/env python3

from datetime import datetime
from random import shuffle
import json

from flask import Flask, request, abort

WORKER_TOKEN = 'secrets!'

workers = {}
solver = None

with open('solvers/gravity.solve') as f:
    solver = f.read()

def parse_solver(solver):
    """Parse a solver file contents into a dict

    This is incomplete, currently only loading patterns and depth. As
    additional information is needed from solvers, this will be expanded.

    """
    ret = {
        'patterns': [],
    }

    lines = solver.split('\n')

    for line in lines:
        line = line.strip()

        if line.startswith('pattern '):
            ret['patterns'].append(line[8:].strip())
            continue

        if line.startswith('depth '):
            ret['depth'] = int(line[6:].strip())

    return ret

solver_info = parse_solver(solver)
total_assemblies = len(solver_info['patterns']) ** solver_info['depth']
assemblies_unsolved = [a for a in range(total_assemblies)]
next_unsolved_index = 0
total_run = 0

app = Flask(__name__)

@app.route('/')
def index():
    global total_run
    global workers

    total = total_run
    for worker_id, worker in workers.items():
        for a in worker['assemblies_running']:
            total += a['programs_completed']

    return str(total)

@app.route('/worker/status', methods=['POST'])
def worker_status():
    global assemblies_unsolved
    global next_unsolved_index
    global total_run

    req = request.get_json()

    if req['token'] != WORKER_TOKEN:
        print(ac)
        abort(403)

    if req['worker_id'] not in workers:
        workers[req['worker_id']] = {
            'worker_id': req['worker_id'],
            'cores': req['cores'],
            'last_solver_sent': None,
        }

    worker = workers[req['worker_id']]

    worker['last_checkin'] = datetime.now()
    worker['assemblies_running'] = req['assemblies_running']
    worker['assemblies_queued'] = req['assemblies_queued']

    # Remove assemblies from the unsolved list if they've been completed
    if 'assemblies_completed' in req:
        for a in req['assemblies_completed']:
            total_run += a['programs_completed']

        ac = [a['assembly'] for a in req['assemblies_completed']]
        assemblies_unsolved = [a for a in assemblies_unsolved if a not in ac]

    print(assemblies_unsolved)

    ret = {
        'status': 'stopped',
    }

    if solver and len(assemblies_unsolved):
        ret['status'] = 'running'

        if worker['last_solver_sent'] is None or \
           worker['last_solver_sent'] != solver:
            ret['solver'] = solver
            worker['last_solver_sent'] = solver

        # Workers should have twice their number of cores in assemblies
        # running/queued at all times, unless there aren't enough unsolved
        # assemblies left
        current = len(worker['assemblies_running']) + \
                len(worker['assemblies_queued'])

        ideal = worker['cores'] * 2

        needed = min(ideal - current, len(assemblies_unsolved))

        if needed > 0:
            assigned = []

            while len(assigned) < needed:
                if next_unsolved_index > len(assemblies_unsolved) - 1:
                    next_unsolved_index = 0

                assigned.append(assemblies_unsolved[next_unsolved_index])

                next_unsolved_index += 1

            ret['next_assemblies'] = assigned

    return json.dumps(ret)

if __name__ == '__main__':
    app.run(debug=True)
