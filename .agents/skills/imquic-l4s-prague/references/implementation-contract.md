# Implementation Contract

## Deliverables in the IMQUIC repository

Create or modify these conceptual units while following the pinned repository's existing naming conventions:

- `.deps/picoquic-l4s/`: local picoquic clone on `codex/prague-params`.
- `config/l4s-prague.yaml`: versioned named profiles.
- `tools/l4s/prague_profile.py`: dependency-free profile renderer copied from the skill.
- `l4s/manifest.json`: exact revisions, configure flags, compiler, kernel, and profile.
- `l4s/validation-report.md`: evidence and limitations.
- IMQUIC build configuration: accepts the picoquic path rather than requiring an untracked root clone.
- IMQUIC endpoint configuration: controller name plus controller option string.
- Transport metrics adapter: stable snapshot without leaking picoquic internal types.

Do not assume exact source paths. Use `rg` to locate `picoquic_create`, `picoquic_create_and_configure`, congestion-algorithm selection, `picoquic_prague_init`, Prague notification handling, ECN counters, and existing algorithm option parsers.

## Phase A: Baseline and dependency plumbing

1. Run the skill doctor and save its JSON output.
2. Run the unmodified build and tests. A baseline failure blocks feature work until it is characterized.
3. Create the branch and first manifest.
4. Add `.deps/` to `.gitignore`, but track a pin file or manifest.
5. Clone the official picoquic repository at an explicit commit and create the local branch.
6. Modify IMQUIC's Autotools configuration to support `--with-picoquic=PATH`, preserving the existing root-folder default for compatibility.
7. Build picoquic with `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` and picotls enabled, then build IMQUIC against the explicit path.

Commit: `build: support managed picoquic dependency`.

## Phase B: Prague option parsing

Inspect other picoquic congestion controllers for option parsing conventions. Reuse an existing parser when suitable; otherwise add a Prague-local parser with strict duplicate, unknown-key, denominator, overflow, and range checks.

Represent each value as a reduced rational pair or a fixed-point integer. Required semantic ranges:

| Parameter | Meaning | Range |
|---|---|---|
| `alpha_gain` | CE estimator update gain | greater than zero, at most one |
| `ce_response` | CE reduction multiplier | greater than zero, at most one |
| `loss_beta` | window fraction retained after loss | greater than zero, at most one |
| `sudden_ce_threshold` | immediate-CE threshold | greater than zero, at most one |

Write tests before implementation for defaults, valid overrides, missing fields, unknown fields, duplicate fields, zero denominators, out-of-range values, and arithmetic overflow.

The default state must derive from constants that reproduce the pinned implementation. Do not silently change initial congestion window, pacing, recovery transitions, ECN validation, or fallback behavior.

Commit: `feat: parameterize picoquic Prague response`.

## Phase C: Congestion response tests

Add deterministic tests around the smallest testable Prague state transition. Cover:

- no CE leaves the estimator and congestion window on the upstream path;
- a known marked fraction updates alpha using `alpha_gain`;
- CE reduction scales with `ce_response`;
- loss reduction retains `loss_beta` of the previous window;
- default parameters match the pre-patch expected values;
- invalid options fail connection or context setup with an actionable error.

Avoid floating point in the controller. In tests, calculate expected values using wide integers and explicit rounding rules.

Commit: `test: cover configurable Prague dynamics`.

## Phase D: IMQUIC configuration and metrics

Add a typed controller choice with at least `default`, `reno`, `bbr`, and `prague`. Pass the rendered Prague option string through the public picoquic context or connection configuration API. Do not set controller globals after active connections already exist unless the picoquic API explicitly defines that behavior.

Expose a read-only snapshot with stable primitive fields:

```c
struct imquic_transport_metrics {
    uint64_t smoothed_rtt_us;
    uint64_t min_rtt_us;
    uint64_t congestion_window_bytes;
    uint64_t bytes_in_flight;
    uint64_t pacing_rate_bytes_per_second;
    uint64_t ect1_packets;
    uint64_t ce_packets;
    uint32_t prague_alpha_numerator;
    uint32_t prague_alpha_denominator;
};
```

Adapt names and types to project conventions. If picoquic lacks a public accessor, add one in the managed fork. IMQUIC must not include `picoquic_internal.h`.

Add tests proving controller and option propagation and a smoke test that creates a Prague connection with the default profile.

Commit: `feat: expose Prague configuration and metrics in IMQUIC`.

## Phase E: Documentation and reproducibility

The manifest must include:

- IMQUIC commit and branch;
- picoquic upstream URL, pinned commit, local branch, and dirty status;
- picotls commit when fetched separately;
- profile name and normalized rational values;
- compiler, CMake, Autoconf, Automake, kernel, and `tc` versions;
- exact build and validation commands.

Large packet captures belong under `results/l4s/`, which should be ignored. Track checksums and a small text or JSON summary instead.

Commit: `docs: record IMQUIC L4S experiment workflow`.
