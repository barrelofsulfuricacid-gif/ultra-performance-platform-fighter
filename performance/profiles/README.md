# Performance profiles

Milestone and large-change analyses live in one directory per label. Raw Tracy
captures, OS-profiler recordings, logs, and capture-tool builds remain under
`performance/local/profiles/` unless an owner explicitly approves a durable
snapshot.

Run:

```sh
./tools/capture_profile.sh M3
```

The command builds the profile-only benchmark with pinned Tracy 0.13.1,
captures canonical benchmark zones through the matching command-line capture
utility, and attempts the platform profiler (`perf` on Linux or `xctrace` on
macOS). A missing or permission-blocked OS profiler is recorded explicitly in
the machine-readable manifest rather than silently treated as a capture.

On WSL, `tools/workflow.sh profile` selects Tracy's lower-resolution timer
fallback because the WSL kernel can mask invariant-TSC support. Native Linux,
macOS, and Windows profile builds keep Tracy's hardware-timer path unless
`PF_TRACY_TIMER_FALLBACK=ON` is set explicitly. Every profile manifest records
the resolved timer policy.
