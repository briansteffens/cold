#!/usr/bin/env python3

from datetime import datetime
from random import shuffle
import sys
import json
import math
import glob
import statistics
from functools import wraps

from flask import Flask, request, abort, make_response, Response, session

def requires_auth(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        auth = request.authorization
        if not auth or auth.password != WORKER_TOKEN:
            return Response(
                'Could not verify your access level for that URL.\n'
                'You have to login with proper credentials', 401,
                {'WWW-Authenticate': 'Basic realm="Login Required"'})
        return f(*args, **kwargs)
    return decorated

if len(sys.argv) != 2:
    print('Usage:')
    print('cluster/server.py [token]')

WORKER_TOKEN = sys.argv[1]

# Global state object
class ServerState(object):
    def __init__(self):
        self.status = 'stopped'
        self.workers = []
        self.solver = None
        self.total_assemblies = None
        self.assemblies_unsolved = None
        self.next_unsolved_index = 0
        self.total_run = 0
        self.solutions = []
        self.solvers = []

state = ServerState()

# Reset the cluster state and set a new solver
def reset(state, solver_text):
    state.solver = solver_text

    lines = solver_text.split('\n')

    state.patterns = []
    state.depth = 1

    for line in lines:
        line = line.strip()

        if line.startswith('pattern '):
            state.patterns.append(line[8:].strip())
            continue

        if line.startswith('depth '):
            state.depth = int(line[6:].strip())

    state.total_assemblies = len(state.patterns) ** state.depth
    state.assemblies_unsolved = [a for a in range(state.total_assemblies)]
    state.next_unsolved_index = 0
    state.total_run = 0
    state.solutions = []

    for worker in state.workers:
        worker['run_rate'] = None
        worker['assemblies_completed'] = []
        worker['run_samples'] = []
        worker['assemblies_running'] = []
        worker['assemblies_queued'] = []
        worker['programs_run'] = 0

# Load the solvers from disk
for solver_fn in glob.glob('solvers/*.solve'):
    name = solver_fn.replace('solvers/', '').replace('.solve', '')
    with open(solver_fn) as f:
        state.solvers.append({
            'name': solver_fn.replace('solvers/', '').replace('.solve', ''),
            'text': f.read(),
        })

# Default to the first solver in the list
if len(state.solvers):
    reset(state, state.solvers[0]['text'])

app = Flask(__name__)

# Console entrypoint
@app.route('/')
@requires_auth
def index():
    solver_json = ','.join([
        "{name: '" + s['name'] + "'," +
        "text: '" + s['text'].replace('\n', '\\n') + "'}"
        for s in state.solvers])

    with open('cluster/index.html') as f:
        ret = f.read()

        return ret.replace('{solvers}', solver_json)

# Console javascript
@app.route('/view.js')
@requires_auth
def view():
    with open('cluster/view.js') as f:
        return f.read()

# Console stylesheet
@app.route('/style.css')
@requires_auth
def style():
    with open('cluster/style.css') as f:
        res = make_response(f.read(), 200)

    res.headers['Content-Type'] = 'text/css'

    return res

# Console AJAX update endpoint
@app.route('/console_update', methods=['POST'])
@requires_auth
def console_update():
    global state

    req = request.get_json()

    if 'command' in req:
        if req['command'] == 'stop':
            state.status = 'stopped'
            state.solver = None

        elif req['command'] == 'run':
            state.status = 'running'

            # If the solver has changed, reset
            if req['solver'] != state.solver:
                reset(state, req['solver'])

        elif req['command'] == 'disarm':
            state.status = 'disarmed'

        elif req['command'] == 'arm':
            state.status = 'stopped'

        elif req['command'] == 'reset':
            if state.status == 'running':
                state.status = 'stopped'

            reset(state, req['solver'])

    res = {
        'status': state.status,
        'programs_run': state.total_run,
        'workers': [],
        'solutions': state.solutions,
        'unsolved': state.assemblies_unsolved,
        'solved': [],
    }

    for w in state.workers:
        worker_status = 'inactive'

        if w['last_status_sent'] == 'disarmed':
            worker_status = 'disarmed'
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

        res['solved'].extend([{
            'assembly': a['assembly'],
            'programs_completed': a['programs_completed'],
        } for a in w['assemblies_completed']])

    return json.dumps(res)

# Worker update endpoint
@app.route('/worker/status', methods=['POST'])
def worker_status():
    global state

    req = request.get_json()

    if req['token'] != WORKER_TOKEN:
        abort(403)

    worker = next((w for w in state.workers
                   if w['worker_id'] == req['worker_id']), None)

    if worker is None:
        worker = {
            'worker_id': req['worker_id'],
            'cores': req['cores'],
            'last_solver_sent': None,
            'assemblies_completed': [],
            'run_samples': [],
            'run_rate': None,
        }

        state.workers.append(worker)

    worker['last_checkin'] = datetime.now()
    worker['assemblies_running'] = req['assemblies_running']
    worker['assemblies_queued'] = req['assemblies_queued']

    if state.status == 'running':
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
                    state.total_run += ac_new['programs_completed']
                    state.solutions.extend(ac_new['solutions'])

            ac = [a['assembly'] for a in req['assemblies_completed']]
            state.assemblies_unsolved = [a for a in state.assemblies_unsolved
                                         if a not in ac]

    # End execution
    if state.status != 'disarmed' and (state.assemblies_unsolved is None or
                                       len(state.assemblies_unsolved) == 0):
        state.status = 'stopped'

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
    if len(worker['run_samples']) <= 1 or state.status != 'running':
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

    # Respond to worker
    ret = {
        'status': state.status,
    }

    worker['last_status_sent'] = state.status

    # Send the solver if needed
    if 'first_status' in req and req['first_status'] or \
       worker['last_solver_sent'] is None or \
       worker['last_solver_sent'] != state.solver:
        ret['solver'] = state.solver
        worker['last_solver_sent'] = state.solver

    if ret['status'] == 'running':
        # Workers should have twice their number of cores in assemblies
        # running/queued at all times, unless there aren't enough unsolved
        # assemblies left
        current = len(worker['assemblies_running']) + \
                  len(worker['assemblies_queued'])

        ideal = worker['cores'] * 2

        needed = min(ideal - current, len(state.assemblies_unsolved))

        if needed > 0:
            assigned = []

            while len(assigned) < needed:
                if state.next_unsolved_index > \
                   len(state.assemblies_unsolved) - 1:
                    state.next_unsolved_index = 0

                assigned.append(
                        state.assemblies_unsolved[state.next_unsolved_index])

                state.next_unsolved_index += 1

            ret['next_assemblies'] = assigned

    return json.dumps(ret)

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)
