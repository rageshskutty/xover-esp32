# Host tests

## Registered CTest coverage

The current CMake project registers the DSP block tests listed in `host/CMakeLists.txt`. The graph-era `dspgraph` component, its tests and `validate_cli` were deleted; the chain-v1 config loader gets its own tests with its card.

`test_sos_response.c` exercises the existing public `dsp_sos_eval()` API from `dspblocks.h` and is registered as a standalone CTest test.

## Orphaned source files (not part of the current build)

The following files are present under `host/tests/` but are intentionally not registered because their required headers or implementation modules are absent from this branch. Do not add them to CMake until the corresponding module is restored or the test is deliberately ported:

| Test source | Missing include / dependency | Classification |
|---|---|---|
| `test_app_state_machine.c` | `app_state_machine.h` | Orphaned application state-machine test |
| `test_config_slot.c` | `config_slot.h` | Orphaned configuration-slot test |
| `test_request_bounds.c` | `xoweb_bounds.h` | Orphaned web request-bounds test |
| `test_s7_ramp.c` | `dsp_s7_ramp.h` | Orphaned ramp helper test |
| `test_spsc_ring.c` | `audiosrc_ring.h` | Orphaned audio ring test |
| `test_tdm_pack.c` | `tdm_pack.h` | Orphaned TDM packer test |
| `test_token_auth.c` | `xoweb_auth.h` | Orphaned web token-auth test |

This inventory is based on source/header presence in the inspected branch tree. It is not a claim that these modules are unnecessary; preserve the source files until a maintainer decides whether to restore, port, or remove them.

## Test authoring

Add deterministic unit tests for current DSP APIs. Compare filter response against an independent numerical reference (NumPy/SciPy) and record tolerances. Never use precomputed fixture metadata as a substitute for calculating response values.
