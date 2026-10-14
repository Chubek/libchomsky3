# Package System

Jiterati packages use the `.jpkg` extension and are handled by `jiterati-cli package`. The current implementation uses a `JPKG1` archive envelope and Zstandard helpers from `zstd/zstd.hpp`.

## Package Layout
```text
META-INF/
  manifest.json
BE/
Plugin/
Pass/
Docs/
Examples/
Resources/
```

Optional:
```text
Tests/
Licenses/
Scripts/
```

## Manifest
Required fields for practical use:
```json
{
  "name": "example",
  "version": "0.1.0",
  "description": "...",
  "plugins": [],
  "passes": [],
  "backends": [],
  "files": []
}
```

Every installed file should appear in `files`.

## CLI Commands
```sh
jiterati-cli package --pack package.sh --out Foo.jpkg
jiterati-cli package --unpack Foo.jpkg --out DIR
jiterati-cli package --validate Foo.jpkg
jiterati-cli package --view-manifest Foo.jpkg
jiterati-cli package --list-plugins Foo.jpkg
jiterati-cli package --list-passes Foo.jpkg
jiterati-cli package --list-backends Foo.jpkg
jiterati-cli package --install Foo.jpkg
jiterati-cli package --remove NAME
jiterati-cli package --upgrade Foo.jpkg
jiterati-cli package --list-installed
jiterati-cli package --verify NAME
jiterati-cli package --repair
```

## Install Root
`JITERATI_HOME` overrides the install root. Default:
```text
~/.local/jiterati
```

Expected subdirectories:
```text
bin/ plugins/ passes/ backends/ packages/ cache/ logs/ docs/ examples/
```

## Safety Rules
- Archive paths must be relative and safe.
- Packages must contain `META-INF/manifest.json`.
- Installation must not write outside `JITERATI_HOME`.
- Validation must reject malformed manifests and unsafe paths.
