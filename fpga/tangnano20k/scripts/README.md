# Hardware build scripts

The physical build script is intentionally pending because this environment
does not contain a verified board constraint set or Gowin/Apicula toolchain.
When those inputs exist, the script must identify the exact device, consume the
checked constraints, and keep generated files under `build/tangnano20k/`.
