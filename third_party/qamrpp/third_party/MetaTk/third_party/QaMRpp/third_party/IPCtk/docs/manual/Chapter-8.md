# Chapter-8: Developer Guide and Contribution

## Standards

- preserve header-only API stability unless version-gated.
- keep DSL semantics separate from target-specific lowering.
- avoid edits in external/library vendored paths.

## Change Process

- update docs with API/DSL changes.
- update `dest/*.itkd` when mapping contracts change.
- keep `ipctk.pc.in` metadata aligned with install layout.
- maintain `examples/` and build integration consistency.

## Release Checklist

- configure/build clean;
- tests compile and execute;
- docs generate;
- install layout verified;
- pkg-config file validated.
