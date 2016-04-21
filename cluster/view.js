var View = React.createClass({
  getInitialState: function() {
    return {
      server: null,
      solvers: solvers,
      solver_selection: 0,
    };
  },
  sendToServer: function(data) {
    let xhttp = new XMLHttpRequest();

    let that = this;

    xhttp.addEventListener('load', function() {
      console.log(JSON.parse(this.responseText));
      that.setState({
        server: JSON.parse(this.responseText),
      });
    });

    xhttp.open('POST', '/console_update', true);
    xhttp.setRequestHeader('Content-Type', 'application/json');
    xhttp.send(JSON.stringify(data));
  },
  serverUpdate: function() {
    this.sendToServer({});
  },
  componentDidMount: function() {
    setInterval(this.serverUpdate, 1000);
  },
  run: function() {
    this.sendToServer({'command': 'run'});
  },
  stop: function() {
    this.sendToServer({'command': 'stop'});
  },
  pause: function() {
    this.sendToServer({'command': 'pause'});
  },
  unpause: function() {
    this.sendToServer({'command': 'unpause'});
  },
  reset: function() {
    this.sendToServer({
      'command': 'reset',
      'solver': this.refs.solver.value,
    });
  },
  selectSolver: function() {
    let solver = null;

    for (let s of this.state.solvers) {
      if (s.name === this.refs.solverList.value) {
        solver = s;
        break;
      }
    }

    if (solver === null) {
      console.log('No solver found');
    }

    this.refs.solver.value = solver.text;
  },
  render: function() {
    if (this.state.server === null) {
      return (<div>connecting..</div>);
    }

    let workers = [];
    let total_run_rate = 0;

    for (let worker of this.state.server.workers) {
      workers.push(
        <tr key={worker.worker_id}>
          <td>{worker.worker_id}</td>
          <td>{worker.cores}</td>
          <td>{worker.run_rate}/sec</td>
          <td>{worker.programs_run}</td>
          <td>{worker.assemblies_completed}</td>
          <td>{worker.status}</td>
        </tr>
      );

      total_run_rate += worker.run_rate;
    }

    let status = this.state.server.status;

    let buttons = [];

    if (status === 'running') {
      buttons.push(<button key='stop' onClick={this.stop}>stop</button>);
    }

    if (status === 'stopped' || status === 'paused') {
      buttons.push(<button key='run' onClick={this.run}>run</button>);
    }

    if (status === 'running' || status === 'stopped') {
      buttons.push(<button key='pause' onClick={this.pause}>pause</button>);
    }

    if (status === 'paused') {
      buttons.push(<button key='unpause' onClick={this.unpause}>
          unpause</button>);
    }

    let solverDefault =
      '\n# G((m1 * m2) / (r ^ 2))\n\n' +
      'depth 4\n\n' +
      'precision D1E2\n\n' +
      'constant D6.67408E-11 # Gravitational constant\n' +
      'constant D2E0\n\n' +
      'pattern mul\n' +
      'pattern div\n' +
      'pattern exp\n\n' +
      'input m1\n' +
      'input m2\n' +
      'input d\n\n' +
      '# Earth mass, moon mass, distance, newtons\n' +
      'case D5.972E24 D7.34767309E22 D3.8E8 D2.028121E20';

    solverDefault =
      'depth 1\npattern add\ninput z\ncase i2 i4\ncase i3 i6\n';

    let solutions = [];
    for (let i = 0; i < this.state.server.solutions.length; i++) {
        solutions.push(<pre key={i}>{this.state.server.solutions[i]}</pre>);
    }

    let solved = [];
    for (let a of this.state.server.solved) {
      solved.push(
        <span key={a.assembly}>
          {a.assembly} [{a.programs_completed}]&nbsp;
        </span>
      );
    }

    let solverOptions = [];
    for (let i = 0; i < this.state.solvers.length; i++) {
      let solver = this.state.solvers[i];

      solverOptions.push(<option key={solver.name}>{solver.name}</option>);
    }

    return (
      <div id="container">
        <div id="input">
          <select ref="solverList" onChange={this.selectSolver}>
            { solverOptions }
          </select>
          <textarea ref="solver" defaultValue={solverDefault}></textarea>
          <button onClick={this.reset}>set solver</button>
        </div>
        <div id="cluster">
          status: { status } { buttons }
          <br />
          programs run: { this.state.server.programs_run }
          <br />
          assemblies run: { this.state.server.solved.length }
          <br />
          rate: { total_run_rate }/sec
          <br />
          <table border="1">
            <thead>
              <tr>
                <th>id</th>
                <th>cores</th>
                <th>rate</th>
                <th>programs run</th>
                <th>assemblies completed</th>
                <th>status</th>
              </tr>
            </thead>
            <tbody>
              { workers }
            </tbody>
          </table>
          unsolved assemblies:
          <div>
            {this.state.server.unsolved.join(' ')}
          </div>
          solved assemblies:
          <div>
            {solved}
          </div>
        </div>
        <div id="solutions">
          { solutions }
        </div>
      </div>
    );
  },
});

var view = ReactDOM.render(<View />, document.getElementById('view'));
