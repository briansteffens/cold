# Usage

Find a program for E=MC^2:

```bash
bin/cold solve solvers/emc2.solve
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
