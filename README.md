cold
====

# Downloading and compiling

You'll need git to download the source code and gcc to compile it.

In Debian-distros:

```bash
sudo apt-get update
sudo apt-get install git
sudo apt-get install build-essential
git clone https://github.com/briansteffens/ccold
cd ccold
make
```

# Usage

Find a program for E=MC^2:

```bash
bin/cold solve solvers/emc2.solve
```

Find all programs for E=MC^2:

```bash
bin/cold solve solvers/emc2.solve --all
```

Run a program with an input argument of 2:

```bash
bin/cold run example/basic.cold 2
```

View the line-by-line debug output including variable dumps:

```bash
cat output/debug
```

# Cluster

Cold has a clustering system for spreading the work of a solver across multiple
computers. The clustering system is written in Python and operates over HTTP.
HTTPS is possible in combination with a reverse proxy web server like nginx or
apache to terminate SSL.

The structure of a cold cluster is as follows:

- **Server** - This facilitates communication between the user and the
  worker machines. The user controls the cluster through a web-based console.
  Implemented in `cluster/server.py`.

- **Worker(s)** - Worker instances poll a server for work to be done, shell out
  to the cold binary `bin/cold`, and send results back to the server.
  Implemented in `cluster/worker.py`.

## Deployment on Linode

*Note: this deployment option currently uses HTTP for communication between
workers, the server, and the console. Don't reuse a password you use for other
services as the token and don't enter any information into the console you
don't want getting out.*

### Server

To bring up a cold cluster server on Linode, use this
[StackScript](https://www.linode.com/stackscripts/view/19184). Take note of the
value you set for the `token` field, as it will be needed for logging into the
web console and for subscribing cluster workers to this server. If you want to
work off of a fork or particular branch, customize the `git_source` and
`git_branch` fields.

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
