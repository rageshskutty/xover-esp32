# Compile stubs for planned headers

`tools/check_fixtures.py` puts this directory **last** on the include path, so
the real header wins as soon as its task lands. Each stub is a declaration-only
copy of the contract in the matching `docs/engineering/prompts/T*.md`; delete
the stub when the real header is merged. Stubs are never linked and never
define behavior.
