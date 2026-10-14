# Analysis Passes

Analysis passes compute facts without semantic mutation.

## Pass API
`include/Jiterati-Pass.hpp` defines:
```cpp
class Pass { virtual std::string name() const = 0; };
class FunctionPass : public virtual Pass { virtual void run(Function&) = 0; };
class ModulePass : public virtual Pass { virtual void run(Module&) = 0; };
class AnalysisPass : public virtual Pass {};
class TransformPass : public virtual Pass {};
```

`PassManager` owns passes and runs module passes over a module.

## Bundled Analyses
`Passlib/` contains:
- `CFGAnalysis.cpp`
- `DominatorAnalysis.cpp`
- `LivenessAnalysis.cpp`

These are implementation units, not yet exposed as a rich registry API in the public header.

## Analysis Discipline
- Do not mutate IR semantics.
- Keep cached results scoped to the analyzed function/module.
- Define invalidation conditions.
- Prefer deterministic traversal order.
- Avoid target-specific facts in generic analyses.

## JPL Pipeline Parser
```cpp
PassManager parse_jpl_pipeline(std::string_view source);
```

Current implementation parses whitespace/semicolon-separated pass names into no-op module passes. Treat it as syntax scaffolding, not a full pass registry.
