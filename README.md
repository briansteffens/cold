# Usage

Find a program for E=MC^2:

```bash
bin/cold solve --output-all solvers/emc2.solve
```

View all programs generated in the process:

```bash
cat output/generated_programs
```

View the solution program:

```bash
cat output/solution.cold
```

Run the solution program (with an input argument of 0.25):

```bash
bin/cold run output/solution.cold f0.25
```

View the line-by-line debug output including local dumps:

```bash
cat output/debug
```

# Cluster

## Deployment on Linode

### Server

To bring up a cold cluster server on Linode, use this
[StackScript](https://www.linode.com/stackscripts/view/19184). Take note of the
value you set for the `token` field, as it will be needed for logging into the
web console and for subscribing cluster workers to this server. If you want to
work off of a fork or particular branch, customize the `git_source` and
`git_branch` fields.

*Note: this will bring up a cluster server that uses HTTP. Don't use a password
from another service as your token as it's not secure.*

### Workers

To enroll worker instances in a cluster server, use this
[StackScript](https://www.linode.com/stackscripts/view/19130). Set the
`server_url` to the public URL of the server you want to connect to
(example: `http://192.168.1.123/`). Use the same `token` you used to create the
server. You can also give each worker a descriptive name with `worker_id` and
a max number of threads to devote to program execution.

### Console

Once you have a server up and some number of workers, connect to the public
URL of the server with a web browser (example: http://192.168.1.123/) to
control and monitor the cluster.
