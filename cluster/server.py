#!/usr/bin/env python3

from datetime import datetime
from random import shuffle
import json
import math
import statistics

from flask import Flask, request, abort, make_response

WORKER_TOKEN = 'secrets!'

workers = []
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
status = 'running'

app = Flask(__name__)

@app.route('/')
def index():
    with open('cluster/index.html') as f:
        return f.read()
#    global total_run
#    global workers
#
#    total = total_run
#    for worker_id, worker in workers.items():
#        for a in worker['assemblies_running']:
#            total += a['programs_completed']
#
#    return str(total)

@app.route('/view.js')
def view():
    with open('cluster/view.js') as f:
        return f.read()

@app.route('/style.css')
def style():
    with open('cluster/style.css') as f:
        res = make_response(f.read(), 200)

    res.headers['Content-Type'] = 'text/css'

    return res

@app.route('/console_update', methods=['POST'])
def console_update():
    global status

    req = request.get_json()

    if 'command' in req:
        if req['command'] == 'stop':
            status = 'stopped'
            solver = None

        elif req['command'] == 'run':
            status = 'running'

            if 'solver' in req:
                solver = req['solver']

        elif req['command'] == 'pause':
            status = 'paused'

        elif req['command'] == 'unpause':
            status = 'stopped'

    res = {
        'status': status,
        'programs_run': total_run,
        'workers': [],
    }

    for w in workers:
        worker_status = 'inactive'

        if w['last_status_sent'] == 'paused':
            worker_status = 'paused'
        elif (datetime.now() - w['last_checkin']).total_seconds() < 5:
            worker_status = 'active'

        res['workers'].append({
            'worker_id': w['worker_id'],
            'cores': w['cores'],
            'run_rate': w['run_rate'],
            'assemblies_completed': len(w['assemblies_completed']),
            'programs_run': w['programs_run'],
            'status': worker_status,
        })

    return json.dumps(res)

@app.route('/worker/status', methods=['POST'])
def worker_status():
    global assemblies_unsolved
    global next_unsolved_index
    global total_run
    global status

    req = request.get_json()

    if req['token'] != WORKER_TOKEN:
        print(ac)
        abort(403)

    worker = next((w for w in workers if w['worker_id'] == req['worker_id']),
            None)

    if worker is None:
        worker = {
            'worker_id': req['worker_id'],
            'cores': req['cores'],
            'last_solver_sent': None,
            'assemblies_completed': [],
            'run_samples': [],
            'run_rate': None,
        }

        workers.append(worker)

    worker['last_checkin'] = datetime.now()
    worker['assemblies_running'] = req['assemblies_running']
    worker['assemblies_queued'] = req['assemblies_queued']

    # Remove assemblies from the unsolved list if they've been completed
    if 'assemblies_completed' in req:
        for ac_new in req['assemblies_completed']:
            found = False

            for ac_old in worker['assemblies_completed']:
                if ac_old['assembly'] == ac_new['assembly']:
                    found = True
                    break

            if not found:
                worker['assemblies_completed'].append(ac_new)
                total_run += ac_new['programs_completed']

        ac = [a['assembly'] for a in req['assemblies_completed']]
        assemblies_unsolved = [a for a in assemblies_unsolved if a not in ac]

    # Calculate run sample
    worker['programs_run'] = sum(a['programs_completed']
        for a in worker['assemblies_completed'] + worker['assemblies_running'])

    worker['run_samples'].append({
        'sample': worker['programs_run'],
        'timestamp': datetime.now(),
    })

    if len(worker['run_samples']) > 3:
        del worker['run_samples'][0]

    # Calculate run rate
    if len(worker['run_samples']) <= 1:
        worker['run_rate'] = None
    else:
        run_rates = []

        for i in reversed(range(len(worker['run_samples']) - 1)):
            earlier = worker['run_samples'][i]
            later = worker['run_samples'][i + 1]

            run_rates.append(
                later['sample'] - earlier['sample'] /
                (later['timestamp'] - earlier['timestamp']).total_seconds())

        worker['run_rate'] = math.ceil(statistics.mean(run_rates))

    print(assemblies_unsolved)

    # Respond to worker
    ret = {
        'status': status,
    }

    worker['last_status_sent'] = status

    # Send the solver if needed
    if 'first_status' in req and req['first_status'] or \
       worker['last_solver_sent'] is None or \
       worker['last_solver_sent'] != solver:
        ret['solver'] = solver
        worker['last_solver_sent'] = solver

    if ret['status'] == 'running':
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
