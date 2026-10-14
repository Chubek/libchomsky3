# SERDE Usage Plan

This project should integrate Serde at these seams:

1. IPC-L transform steps
- `decode(...)` and `encode(...)` operations should call codec policy hooks.
- Per-message type tags should map to serializer registrations.

2. Runtime channel boundaries
- `runtime::Channel` typed send/receive should encode/decode payloads at transport edges.
- Frame encoders/decoders should own length-prefix and schema version metadata.

3. Backend emission
- ITKD rules should emit target-language serde calls for message marshalling.
- Capability validation should require serde availability for typed payload paths.

4. Compile pipeline
- Lowering should annotate IR steps with selected codec strategy.
- Validation should check that every typed message has a codec binding.

5. Testing
- Round-trip tests for each message type.
- Cross-backend compatibility tests for generated wire format.
