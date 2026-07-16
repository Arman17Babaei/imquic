---
name: imquic-l4s-prague
description: Use when an IMQUIC repository needs Prague congestion control, L4S experiments, a managed picoquic fork, tunable alpha_gain or beta behavior, or ECT(1) and CE validation.
---

# IMQUIC L4S Prague

## Overview

Patch IMQUIC through one workspace-managed picoquic clone. Keep MoQ semantics in IMQUIC and Prague/L4S mechanics in picoquic. Preserve upstream defaults unless a named experiment profile overrides them.

## Start Here

1. Run `python3 <skill>/scripts/doctor.py . --json`.
2. Stop when the repository is not IMQUIC, Git is unavailable, or unrelated changes exist. Report the exact obstruction; do not stash, reset, or overwrite it.
3. Create `codex/imquic-l4s-prague`, adding a numeric suffix when needed. Make small commits and never push.
4. Read `references/implementation-contract.md` before editing.

## Required Workflow

### 1. Pin and isolate

Record the IMQUIC revision, toolchain, kernel, and selected picoquic upstream revision. Clone picoquic into `.deps/picoquic-l4s`; treat this local clone as the managed fork. Do not replace an existing dependency directory or create a remote fork without explicit instruction.

### 2. Work test-first

For each behavior, write a failing test, run it, implement the smallest change, and rerun the relevant suite. Separate commits for dependency plumbing, Prague parameters, IMQUIC configuration, metrics, validation, and documentation.

### 3. Patch Prague in picoquic

Inspect the pinned source rather than assuming filenames. Add runtime rational parameters named `alpha_gain`, `ce_response`, `loss_beta`, and `sudden_ce_threshold`. Parse them from the congestion-controller option string. Use integer or fixed-point arithmetic. Missing options must reproduce the pinned upstream behavior bit-for-bit where practical.

Do not replace the dynamic Prague alpha estimator with a fixed alpha. `alpha_gain` controls its update rate; `ce_response` controls CE-based window reduction; `loss_beta` controls retained window after loss.

### 4. Expose the configuration through IMQUIC

Copy `assets/l4s-prague.yaml` to `config/l4s-prague.yaml` and the profile helper to `tools/l4s/prague_profile.py`. Render a selected profile with:

```bash
python3 tools/l4s/prague_profile.py render-options config/l4s-prague.yaml PROFILE
```

Add an explicit IMQUIC congestion-controller selection and option-string path. Prefer an Autotools `--with-picoquic=PATH` option over a hard-coded root dependency. Expose a narrow metrics snapshot for RTT, congestion window, bytes in flight, pacing, ECT(1), CE, and Prague alpha; do not make IMQUIC include picoquic internal headers.

### 5. Validate honestly

Build picoquic with position-independent code and picotls, then build IMQUIC and run relevant tests. On Linux with sufficient privileges, follow `references/l4s-validation.md` to attempt namespaces, DualPI2, packet capture, and instrumented CE feedback checks.

Packet capture proves IP ECT(1) and CE marking. It does not expose encrypted QUIC ACK ECN counters; prove CE feedback through picoquic metrics, qlog, or temporary test instrumentation. When root, `tc`, DualPI2, namespaces, or capture support is absent, use the documented fallback and state that packet-level L4S validation was not performed.

## Output Contract

Create a validation report containing exact commits, profile values, commands, test results, ECT(1) evidence, CE evidence, congestion-window response, unavailable capabilities, and remaining caveats. Cite the papers and standards in `references/scientific-sources.md`.

## Stop Conditions

Stop rather than guessing when the working tree is dirty, the pinned source differs materially from the implementation contract, upstream tests fail before changes, a parameter cannot preserve default behavior, or network evidence contradicts instrumentation. Diagnose the mismatch before editing around it.

## Common Mistakes

| Mistake | Correct action |
|---|---|
| Calling a fixed setting “Prague alpha” | Keep alpha dynamic; tune `alpha_gain` or `ce_response` |
| Patching IMQUIC only | Put congestion-control math in the managed picoquic fork |
| Reading picoquic internal structs from IMQUIC | Add a narrow public accessor or adapter snapshot |
| Claiming L4S from controller selection | Verify ECT(1), CE, feedback, and response |
| Treating a build-only result as packet validation | Label it as the non-privileged fallback |
