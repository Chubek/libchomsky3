IDL Values and Structures
=========================

Subjects covered
----------------

1. Scalar value categories.
2. Null and boolean representation.
3. Integer/number handling constraints.
4. String storage and escape materialization.
5. List value parse behavior.
6. Record value parse behavior.
7. Nested list/record recursion limits.
8. Trailing delimiter rejection policy.
9. Value node ownership and teardown semantics.
10. Debug dump expectations for complex values.

Validation focus
----------------

- Verify list separator errors produce actionable diagnostics.
- Verify record key/value boundaries preserve source ranges.
- Verify nested values round-trip through debug dump.
