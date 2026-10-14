# Chapter-6: Bindings and Integration

## SWIG Stack

- interface file: `bindings/IPCtk.i`.
- generation driver: `bindings/GenerateBindings.py`.

## Supported Languages

- Python;
- Ruby.

## Workflow

- generate wrapper: `python3 bindings/GenerateBindings.py --lang python`.
- compile wrapper in downstream package build.

## Extension Points

- typemap specialization;
- selective symbol export;
- target-runtime packaging hooks.
