# M3 profile capture

Tracy 0.13.1 capture: **pass**. The trace contains canonical benchmark
scenario zones and frame marks.

- Commit: `55619230599dddff2833bfc6e90e1bf6172e166c`
- Dirty tree: `false`
- Scenario zones and frame marks: present
- Profile workload: all 13 canonical slots exercised
- Measured scenarios: 9
- Explicitly unavailable scenarios: 4
- Platform-profiler claim: none

The profiled workload completed all 13 canonical scenario slots, with nine
measured and four explicitly unavailable, using a 100 ms target and 15
repetitions. The raw trace, exact environment metadata, and tool build remain
local by default, avoiding both privacy disclosure and per-commit artifact
recursion.
