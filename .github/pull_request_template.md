<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
## Summary

<!-- What changed and why? -->

## Validation

- [ ] `cmake -S . -B build -DVOX_BUILD_TESTS=ON`
- [ ] `cmake --build build`
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] `cargo test --workspace`
- [ ] Determinism/replay impact checked
- [ ] License/provenance checked

## Contributor sign-off

By submitting this pull request, I certify that my commits carry the
Developer Certificate of Origin sign-off (`git commit -s`).
