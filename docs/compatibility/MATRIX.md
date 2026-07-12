<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Compatibility matrix

| State | Meaning |
|---|---|
| PLANNED | Roadmap target only |
| BUILDS | Reproducible build exists |
| RUNS | Boots and completes smoke scenario |
| VERIFIED | Acceptance evidence is recorded |
| UNSUPPORTED | Explicitly outside current profile |

`v0.0.1` starts with Linux x86-64. Every future row must name the exact OS,
CPU, GPU/API, compiler, build, boot, input, render, match, and performance
evidence before it becomes `VERIFIED`.
