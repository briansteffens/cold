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
    this.sendToServer({
      'command': 'run',
      'solver': this.refs.solver.value,
    });
  },
  stop: function() {
    this.sendToServer({'command': 'stop'});
  },
  disarm: function() {
    this.sendToServer({'command': 'disarm'});
  },
  arm: function() {
    this.sendToServer({'command': 'arm'});
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

    let numberSuffix = function(num) {
      let suffix_index = 0;
      let suffixes = ['', 'k', 'm', 'b', 't', 'q'];

      for (let i = 0; i < suffixes.length; i++) {
        if (num < 1000) {
          return num + suffixes[i];
        }

        let temp = num.toString();
        num = parseInt(temp.substr(0, temp.length - 4));
      }

      return num + suffixes[suffixes.length - 1];
    };

    let workers = [];
    let total_run_rate = 0;

    for (let worker of this.state.server.workers) {
      let run_rate = '';
      if (worker.run_rate) {
        run_rate = numberSuffix(worker.run_rate) + '/sec';
      }

      workers.push(
        <tr key={worker.worker_id}>
          <td>{worker.worker_id}</td>
          <td>{worker.cores}</td>
          <td>{run_rate}</td>
          <td>{worker.programs_run.toLocaleString()}</td>
          <td>{worker.combinations_completed}</td>
          <td>{worker.status}</td>
        </tr>
      );

      total_run_rate += worker.run_rate;
    }

    let run_rate = '';
    if (total_run_rate > 0) {
      run_rate = numberSuffix(total_run_rate) + '/sec';
    }

    let status = this.state.server.status;

    let buttons = [];

    if (status === 'running') {
      buttons.push(<button key='stop' onClick={this.stop}>stop</button>);
    }

    if (status === 'stopped' || status === 'disarmed') {
      buttons.push(<button key='run' onClick={this.run}>run</button>);
    }

    buttons.push(<button onClick={this.reset}>reset</button>);

    if (status === 'running' || status === 'stopped') {
      buttons.push(<button key='disarm' onClick={this.disarm}>disarm</button>);
    }

    if (status === 'disarmed') {
      buttons.push(<button key='arm' onClick={this.arm}>arm</button>);
    }

    let solutions = [];
    for (let i = 0; i < this.state.server.solutions.length; i++) {
        solutions.push(<pre key={i}>{this.state.server.solutions[i]}</pre>);
    }

    let solved = [];
    for (let a of this.state.server.solved) {
      solved.push(
        <span key={a.combination}>
          {a.combination} [{a.programs_completed}]&nbsp;
        </span>
      );
    }

    let solverOptions = [];
    for (let i = 0; i < this.state.solvers.length; i++) {
      let solver = this.state.solvers[i];

      solverOptions.push(<option key={solver.name}>{solver.name}</option>);
    }

    let solverDefault = '';

    if (this.state.solvers.length > 0) {
      solverDefault = this.state.solvers[0].text;
    }

    return (
      <div id="container">
        <div id="input">
          <select ref="solverList" onChange={this.selectSolver}>
            { solverOptions }
          </select>
          <textarea ref="solver" defaultValue={solverDefault}></textarea>
        </div>
        <div id="cluster">
          status: { status } { buttons }
          <br />
          programs run: { this.state.server.programs_run.toLocaleString() }
          <br />
          combinations run: { this.state.server.solved.length }
          <br />
          rate: { run_rate }
          <br />
          <table border="1">
            <thead>
              <tr>
                <th>id</th>
                <th>cores</th>
                <th>rate</th>
                <th>programs run</th>
                <th>combinations completed</th>
                <th>status</th>
              </tr>
            </thead>
            <tbody>
              { workers }
            </tbody>
          </table>
          unsolved combinations:
          <div>
            {this.state.server.unsolved.join(' ')}
          </div>
          solved combinations:
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
