# Chapter 2 - Data Model

The core model includes `ValueType`, `Value`, `Column`, `Schema`, and `Row`.

`Value` stores int64/double/bool/string in a `std::variant`. Schema columns define strict expected runtime types. Runtime insertion and update paths validate values against declared column types.

Column name uniqueness is guaranteed by schema index rebuild. Unknown column access throws explicit runtime errors.
