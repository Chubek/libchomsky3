# MiniZIP Examples

This directory contains 36 focused MiniZIP examples split across:

- `api/` for the conventional builder and extractor API
- `dsl/` for the native C++ pipeline DSL built on `DSLtk.hpp`
- `mzjl/` for MiniZIP Journal language examples and drivers
- `backend/` for direct backend Zstd-subset usage

## API Examples

1. `api/01_simple_build.cpp`
2. `api/02_build_to_explicit_file.cpp`
3. `api/03_add_directory_recursive.cpp`
4. `api/04_add_directory_nonrecursive.cpp`
5. `api/05_add_bytes.cpp`
6. `api/06_add_stream.cpp`
7. `api/07_create_empty_files.cpp`
8. `api/08_remove_item.cpp`
9. `api/09_seal_and_reopen.cpp`
10. `api/10_manifest_ini.cpp`
11. `api/11_deterministic_and_overwrite.cpp`
12. `api/12_build_archive_bytes.cpp`
13. `api/13_extract_list_verify.cpp`
14. `api/14_extract_all_to.cpp`
15. `api/15_extract_bytes.cpp`
16. `api/16_extract_object.cpp`
17. `api/17_open_from_memory.cpp`
18. `api/18_custom_transport.cpp`
19. `api/19_progress_hook.cpp`
20. `api/20_custom_filter.cpp`
21. `api/21_custom_object_serializer.cpp`
22. `api/22_multiple_generated_entries.cpp`

## DSL Examples

23. `dsl/23_dsl_archive_basic.cpp`
24. `dsl/24_dsl_archive_manifest.cpp`
25. `dsl/25_dsl_archive_with_filter_hook.cpp`
26. `dsl/26_dsl_extract_basic.cpp`
27. `dsl/27_dsl_extract_with_deserializer.cpp`

## MZJL Examples

28. `mzjl/28_inline_string_journal.cpp`
29. `mzjl/29_file_journal_loader.cpp`
30. `mzjl/30_journal_minimal.mzjl`
31. `mzjl/31_journal_manifest.mzjl`
32. `mzjl/32_journal_recursive.mzjl`
33. `mzjl/33_journal_with_exclude_and_filter.mzjl`
34. `mzjl/34_journal_diagnostics.cpp`

## Backend Examples

35. `backend/35_backend_zstd_roundtrip.cpp`
36. `backend/36_backend_zstd_inspect.cpp`

## Notes

- The examples assume your build already adds `include/` to the header search path.
- The backend examples use the currently implemented honest Zstd subset:
  raw blocks, RLE blocks, frame inspection, and valid raw-block emission.
- `exclude` and named `filter` directives in `MZJL` are intentionally shown as
  parsed-but-not-fully-executed features so users can see current limitations.
