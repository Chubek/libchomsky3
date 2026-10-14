# Writing Passes

Passes implement analyses or transformations over `Module` or `Function`.

## Minimal Function Pass
```cpp
class MyPass : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "my-pass"; }
    void run(Function& function) override;
};
```

## Minimal Module Pass
```cpp
class MyModulePass : public ModulePass {
public:
    std::string name() const override { return "my-module-pass"; }
    void run(Module& module) override;
};
```

## Pass Manager
```cpp
PassManager manager;
manager.add(std::make_unique<MyModulePass>());
manager.run(module);
```

Current `PassManager::run(Module&)` executes module passes. Function-pass scheduling should be explicit unless a registry/scheduler layer is added.

## Pass Metadata
`specs/passes-specs.yaml` defines documentation fields:
- name;
- category;
- required analyses;
- invalidated analyses;
- preserved analyses;
- inputs/outputs;
- ordering constraints;
- parallel safety;
- determinism.

## Authoring Rules
- State mutation scope precisely.
- Keep analysis and transformation passes separate.
- Preserve deterministic traversal.
- Revalidate after structural mutation.
- Do not encode backend legality in generic passes.
