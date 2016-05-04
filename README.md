cold
====

A constraint-based, brute-force program generator and interpreted language for
fun and experimentation.


# Overview

The basic idea is you feed cold a list of inputs and their corresponding
expected outputs, along with a set of code patterns (instructions) and any
possibly-related constants as hints. Cold will then attempt to brute-force a
programmatic solution that satisfies the given constraints.

To see a list of examples that are currently solvable, see the
[solvers/](solvers/) directory. The [solvers/unsolved/](solvers/unsolved/)
directory contains solvers which are still being worked on.

## Solver file

The solver file provides the overall context for an attempt at generating a
cold program. Here is a basic solver file:

```
# The maximum number of patterns per generated program
depth 2

# A constant value that will be placed in the programs
constant 1

# The patterns cold can use (see below)
pattern add
pattern mul

# The name of the input variable
input x

# Given an input of 2, the program should produce 5
case (2) => 5

# Given an input of 3, the same program should produce 7
case (3) => 7
```


## Patterns

Patterns correspond roughly to instructions but they can technically have more
than one instruction (in order to express conditionals, loops, or even
fundamental algorithms). Patterns are located in the [patterns/](patterns/)
directory.

The solver example above includes the add and mul patterns which look like:

**add**:
```
add !l !l !lc
nxt
```

**mul**:
```
mul !l !l !lc
nxt
```

Each line in a pattern is a cold instruction followed by parameters. `add` and
`mul` refer to the mathematical operations.

`nxt` indicates the location within the pattern that the next pattern in the
*combination* will be inserted.


## Combinations

Cold will read the list of included patterns out of the solver file and use
that to construct a number of *combinations*. In the above example, since there
is a depth of 2 and the patterns `add` and `mul` are included, the following
combinations will be produced:

```
add        add        mul        mul
add        mul        add        mul
```

All possible permutations of all given patterns are attempted, up to the limit
specified by depth.


## Substitution

The cold interpreter will run each combination of patterns independently. Each
line will be interpreted sequentially as would be expected. Where things get a
little less expected is when it encounters a *substitution parameter*
(parameters that start with an exclamation point). A *substitution parameter*
means that when the interpreter reaches that line, the interpreter state will
fork for every possible substitution of that parameter. `!l` includes all local
variables in scope. `!lc` includes all locals as well as all constants. So the
`add` pattern will produce an instruction that will try adding all combinations
of local variables and constants to each other.

Substitutions are done by forking (cloning) the interpreter state at the point
of substitution, creating a tree of related interpreter states. This way not
every possible variant of a program has to be executed in its entirety: the
similar parts of variations can be executed once and then forked where they
begin to diverge.


## Detecting solutions

As the cold interpreter runs, it checks all locals within a program's state
for the expected output in the case being run. If it finds the expected output
in any local variable, it pauses interpretation and runs the same program up
to the current instruction for every other case in the solver file. If the
program produces the expected output in the same local variable for every case,
that program is considered a solution.

The solver file above will produce something like the following:

```cold
def main $x
    add $x $x $x
    add $x $x 1
    ret $x
```

Which, in a prettier language, would look something like:

```python
def main(x):
    return x + x + 1
```


# Types

More types will probably be added as I find uses for them, but for now just
a few primitive types are supported. They also don't work well together, so
you can't really mix-and-match (ints can't be compared to floats). There's no
technical reason for this, the compare() method just only supports the
comparisons I've run into in the solvers I've worked on.


## Type list

**int**

This maps to the `int` type in c. Values of this type have no type hint suffix,
just a basic literal integer.

Examples: `42`, `1024`, `0`

**float**

This maps to the `float` type in c. Values of this type are specified by a type
hint suffix of `f` and can be in either decimal or scientific notation.

Examples: `3.14f`, `6.71e8f`

**long double**

This maps to the `long double` type in c. Values of this type are specified by
a type hint suffix of 'L' and can only be specified in scientific notation.

Examples: `4e80L`, `6.67408e-11L`


## Floating-point comparison

Equality comparison between floating-point types requires a `precision` set
in the solver file, which controls the tolerance (epsilon) by which two values
can differ and still be considered equal. For example:

```
x = 13.5f
y = 13.6f
```

With a precision of 0.15f these two values will be considered equal. With a
precision of 0.05f they will not.


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

Run a long-running solver on multiple threads:

```bash
bin/cold solve solvers/gravity.solve --threads=12
```

View all combinations of a solver without running any of them:

```bash
bin/cold combinations solvers/triangle_area.solve
```

Run a specific combination by index:

```bash
bin/cold solve solvers/triangle_area.solve --combination=2
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


## Manual deployment

Both the server and any workers will require the repository downloaded and
`make` run (see section `Downloading and compiling`).

Both also require python3.


### Server

The server requires flask. To run the server (from the root of the repository),
run the following command, substituting $TOKEN for whatever you want. $TOKEN
will be used to authenticate the workers against the server as well as the
web console.

```bash
cluster/server.py $TOKEN
```


### Workers

On each worker, run the following command:

```bash
cluster/worker.py $SERVER_URL $TOKEN $WORKER_ID $CORES
```

Where:

- **$SERVER_URL** is the base URL of the server
  (default: http://localhost:5000/).
- **$TOKEN** is the same as the token used to start the server.
- **$WORKER_ID** is a label used to identify this worker. This can be whatever
  you want.
- **$CORES** is the number of threads to devote to cold on this worker.


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
