<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# VOX + DIGS governance

VOX + DIGS is developed in public under `GPL-3.0-or-later`. Governance exists
to keep three promises aligned: users can study and change the complete engine,
contributors receive durable credit and a path to responsibility, and platform
or performance claims remain backed by reproducible evidence.

`@claire-moon` is the initial lead maintainer and current CODEOWNERS review
route. CODEOWNERS is a review-routing mechanism, not an assignment of a
contributor's copyright.

## The upstream loop

```text
need -> issue/RFC -> bounded patch -> deterministic evidence -> public review
     -> protected main -> source-first release -> field evidence -> next issue
```

The loop is the project's incentive structure. A platform port, material rule,
optimization, tool, or accessibility improvement becomes more valuable when it
is upstream: it gains shared tests, release distribution, contributor credit,
maintenance help, and reuse by every compatible game or adapter. GPL copyleft
keeps distributed derivatives available under the same user freedoms; the
no-CLA policy keeps contributors from surrendering copyright for access to
that loop.

## Roles

- **Contributor:** reports, documents, designs, tests, reviews, or submits
  signed-off patches.
- **Reviewer:** has a record of accurate, constructive review in a subsystem
  and may provide an approval, but cannot approve their own change alone.
- **Subsystem maintainer:** accepts responsibility for a defined area,
  including review latency, regression response, documentation, provenance,
  and portability impact.
- **Lead/release maintainer:** resolves cross-subsystem decisions, enforces the
  release gate, protects repository credentials/settings, and publishes
  releases from protected `main`.

Roles are earned through sustained accountable work. The lead may grant or
remove repository permissions publicly, with a short reason, after considering
technical judgment, collaboration, security, licensing, and activity. A person
may step down at any time without losing authorship or credit.

## Decisions

Ordinary changes use pull-request review and rough consensus. Reviewers judge
the written contract and evidence, not seniority or employer. The lead
maintainer decides when consensus cannot be reached, but must state the
decision and material tradeoff in the issue or pull request.

An RFC is required for changes to ABI, canonical state, deterministic order,
rules/replay/data formats, dependency policy, world/support profiles,
licensing, or governance. Allow reasonable time for affected maintainers and
port authors to respond. Emergency security or data-loss fixes may land first
with a retrospective record once disclosure is safe.

Accepted design can be revised by a later RFC. Measurements and user evidence
can overturn assumptions; job title, sponsorship, and ownership of faster
hardware cannot substitute for evidence.

## Merge and release authority

Protected `main`, required automated checks, reviewed pull requests, and signed
commits/tags are the intended control plane. A release is source-first and must
include the GPL license, Corresponding Source for distributed binaries,
third-party notices, known limitations, and a compatibility record that
distinguishes build, smoke, and verified status.

A maintainer does not merge their own material change without another qualified
review except for a documented emergency. Release hashes and performance
figures are recorded from the frozen release candidate, never guessed from a
development branch.

## Credit and project resources

Commit authorship is preserved. Release notes name substantive code, design,
documentation, testing, portability, security, accessibility, and provenance
contributions. Co-developed work uses co-author trailers when all participants
agree; DCO sign-off remains required for each contributor's submitted commits.

Donations, grants, sponsored hardware, and paid development do not buy merge or
relicensing authority. If the project accepts material resources, maintainers
record the source, restrictions, intended use, and conflicts of interest in a
public issue or release note unless safety or law prohibits disclosure.

## Forks and compatibility

Forking is a protected GPL freedom. Fork maintainers are welcome to upstream
portable work without transferring copyright. The VOX name, version, and
`VERIFIED` compatibility status must not be used to imply upstream endorsement
of an unreviewed binary or platform result.

Experimental adapters can move quickly out of tree. Upstream acceptance
requires a scalar fallback where relevant, exact toolchain/provenance records,
deterministic equivalence, and the platform evidence defined in the
compatibility matrix.

## Conduct, disputes, and security

Participants follow `CODE_OF_CONDUCT.md`. Technical disagreement stays focused
on contracts, tests, measurements, user impact, and licensing. A contributor
who disagrees with a review may ask the lead for a second reviewer and a public
decision summary.

Security reports follow `SECURITY.md` and may remain private until users can be
protected. Credentials, private user data, embargoed details, and exploit code
are never used as public evidence. Once safe, maintainers publish the minimum
useful advisory, affected versions, fix provenance, and contributor credit.

Changes to this document require an RFC and the same public review as an ABI or
license-policy change.
