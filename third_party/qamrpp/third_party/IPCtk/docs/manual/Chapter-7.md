# Chapter-7: Testing, Fuzzing, Reliability

## Unit Tests

- location: `tests/unit/`.
- role: deterministic behavior/regression coverage.

## Fuzzing

- location: `tests/fuzz/`.
- role: parser/runtime robustness under malformed and adversarial inputs.
- seeds: `tests/fuzz/seed_*.txt`.

## Failure Interpretation

- unit failure: semantic regression or contract drift.
- fuzz crash: parser safety or unchecked edge-state transition.

## Expectations

- new public behavior requires unit coverage.
- parser/runtime boundary changes require fuzz seed updates.
