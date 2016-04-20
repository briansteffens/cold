var View = React.createClass({
  getInitialState: function() {
    return {
      server: null
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
  render: function() {
    if (this.state.server === null) {
      return (<div>connecting..</div>);
    }

    let workers = [];

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

    return (
      <div>
        status: { status } { buttons }
        <br />
        total run: { this.state.server.programs_run }
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
        <br />
        <textarea rows={20} cols={80} ref="solver"
            defaultValue={solverDefault}></textarea>
        <button onClick={this.reset}>reset</button>
        <br />
        { solutions }
      </div>
    );
  },
});

var view = ReactDOM.render(<View />, document.getElementById('view'));
