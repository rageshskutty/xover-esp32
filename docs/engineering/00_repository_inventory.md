# Repository inventory and reuse decisions

**Basis:** GitHub default branch `main`, commit `cd2ae0d598fe7c414f4d128655083b0ffc5ec57d` (2026-11-18). This is a remote source inspection through the GitHub connector; no local checkout or build was available. Statements labeled **Fact** are verified from repository files/tree; **Decision** is a proposed plan; **Open** requires owner/hardware evidence.

## Facts and authority
- `AGENTS.md` governs implementation: 2.1 ESP32-S3 system, three PCM5122 on I2S TDM, 48 kHz, six outputs, DSP core 1; no FIR, 96 kHz, feedback graphs, Bluetooth/AirPlay in v1. Human-only decisions include module memory, IDF/Arduino versions, pins, DAC/straps, protection limits, license and measurements.
- Root `AGENTS.md` in the task wrapper referenced `@RTK.md`, but that file was not present in the task workspace. Repository `AGENTS.md` itself was read and controls repository work.
- Main is a merge-based task history through T06b1: SOS engine, design/evaluation, numeric safety, config schema/validator, graph semantics, coefficient stability and validated-node compilation have landed. Branch listing also shows task branches T02-T06b1. Current main has no audio graph runtime executor, physical audio path, config persistence/HTTP, source decoder, or bring-up implementation in the inventoried component files.
- Existing contracts are marked proposed/frozen at T06 by `docs/api.md`; `docs/design.md`, `docs/PLAN.md`, `configs/schema.json`, `tools/check.py` and `AGENTS.md` are authoritative alongside source.
- A machine-readable exhaustive path listing (all tracked non-submodule blobs) is in `repository_files.json`. This inventory deliberately groups generated golden files and fixture families while listing all source/header files individually.

## Source, header and build inventory
| Path(s) | Current role / public surface | Reuse decision |
|---|---|---|
| `firmware/components/dspblocks/dspblocks.c`, `dsp_sos_rt.c`, `blocks_rt.c` | Filter design/evaluation, SOS processing and gain/delay/polarity/mix/numeric helpers | REUSE_AS_IS subject to existing host regression gates |
| `firmware/components/dspblocks/include/dspblocks.h` | DSP enums, SOS/cascade/state, filter design, RT block, numeric and response APIs | REUSE_AS_IS; extensions require ADR + api.md |
| `firmware/components/dspblocks/CMakeLists.txt` | ESP-IDF component registration for DSP block library | REUSE_AS_IS |
| `firmware/components/dspgraph/dspgraph.c`, `dspgraph_json.c`, `dspgraph_validate.c`, `dspgraph_graph.c`, `dspgraph_build.c` | Bounded JSON parsing, schema-like validation, graph semantics, node compile | REUSE_WITH_EXTENSION: finish graph runtime build/process/ownership and protection integration without replacing validation |
| `dspgraph/include/dspgraph.h` | Current public `dspg_validate(json,len,report)` only | REUSE_WITH_EXTENSION; preserve validation signature |
| `dspgraph/internal/dspgraph_{build,graph,json,validate}.h` | Internal parser, graph validation and compiled-node contracts | REUSE_AS_IS for internal seams; extend only with task-local contracts |
| `dspgraph/CMakeLists.txt` | IDF component dependency/build definition | REUSE_AS_IS |
| `audiosrc/{audiosrc.c,include/audiosrc.h}` | Empty/stub component surface | NEW implementation behind existing boundary |
| `pcm5122/{pcm5122.c,include/pcm5122.h}` | Empty/stub DAC control surface | REUSE_WITH_EXTENSION; hardware values remain human sourced |
| `tdmout/{tdmout.c,include/tdmout.h}` | Empty/stub TDM output surface | REUSE_WITH_EXTENSION; retain component boundary |
| `xoweb/{xoweb.c,include/xoweb.h}` | Empty/stub web control surface | REUSE_WITH_EXTENSION; bounded/authenticated endpoints |
| `firmware/main/main.c` and `firmware/main/CMakeLists.txt` | Application entrypoint, currently skeleton | REUSE_WITH_EXTENSION |
| `firmware/CMakeLists.txt`, component CMake files, `firmware/main/idf_component.yml` | IDF build setup; cJSON is git submodule, main has no active managed dependency yet | REUSE_AS_IS; pin versions via reviewed ADR |
| `firmware/sdkconfig.defaults`, `firmware/partitions.csv` | 8 MB flash config/dual OTA + config slots/storage, PSRAM disabled pending module confirmation | REFACTOR only after measured/module-confirmed partition facts; current 8 MB is an unverified assumption |
| `host/CMakeLists.txt` | Host C11 libraries, warnings-as-errors, unit tests, sanitizer option | REUSE_AS_IS; extend tests with new modules while keeping IDF-independent host build |
| `host/tests/test_*.c` | Unit, analytic response, SOS float32, parser, graph, compile tests | REUSE_AS_IS |
| `host/tests/golden/{butterworth.json,linkwitz_riley.json,rbj_eq.json,golden_vectors.h}` | SciPy/reference data and compiled vectors | REUSE_AS_IS; never regenerate without review of generator and diff |
| `tools/check.py`, `coverage_gate.py`, `gen_golden.py`, `test_scipy_compare.py`, `test_validator_diff.py` | Schema/fixture checks, coverage policy, golden generator, SciPy and differential validation | REUSE_AS_IS |
| `tools/requirements.txt`, `.github/workflows/ci.yml` | Host Python/CMake/CTest/SciPy, ASan/UBSan, coverage CI | REUSE_AS_IS; add target build only after IDF decision |
| `configs/schema.json`, `configs/example_2way_sub.json`, `configs/invalid/*.json` | Version-1 format, sample topology and negative fixtures | REUSE_AS_IS; preserve compatibility and invalid error expectations |
| `data/drivers.yaml`, `data/measurements/README.md` | Driver data shell and measurement instructions, not verified limits | REUSE_WITH_EXTENSION only with human sourced, cited data |
| `web/index.html` | Minimal web page placeholder | REUSE_WITH_EXTENSION after API/auth defined |
| `docs/{PLAN,PROMPT,api,architecture,audio-source,bringup-log,design,hw-pinmap,licenses,questions,requirements}.md`, `docs/adr/*.md` | Vision, design, API, decisions, questions, hardware bring-up and staged plan | REUSE_WITH_EXTENSION; these new docs clarify ownership and executable next tasks |
| `README.md`, `firmware/third_party/README.md`, `host/README.md`, `host/tests/README.md`, `tools/README.md`, `.github/CODEOWNERS`, `.github/pull_request_template.md`, `.gitmodules`, `secrets.example` | Project guidance, ownership, workflow, dependencies and secret placeholders | REUSE_AS_IS |
| `configs/invalid/*.json` (15 fixtures) | Reject cases: unsupported Bessel, unstable biquad, too-low crossover, cycle, duplicate slot, fan-in, gain/Q/order/sample-rate/size/var/property/type errors | REUSE_AS_IS |

No REFACTOR or REPLACE is proposed: functioning implementation is retained. Stub modules receive behavior incrementally within existing boundaries.

## API/dependency and progress evidence
- DSP filter design uses double precision on core 0; RT processing uses float32 SOS state. SOS representation stores `b0,b1,b2,a1,a2,da1,da2`; denominator hi+lo compensation addresses low-frequency float32 precision. Existing design is DF2T.
- `dspg_validate` parses and validates without returning a compiled graph. Internal `dspg_compile_node` compiles one node; graph-level internal validator checks references, cycles, fan-in, slots, variables, limits and section cap.
- cJSON is included as a submodule at `firmware/third_party/cjson`. Host and firmware both link the same implementation. Do not add a second JSON framework.
- GitHub history confirms merged PR/task cadence T02-T06b1 and recent work on float32 accuracy, analytic tests, graph validation, stability and node compilation. This plan is an evolution beyond those completed seams, not a replacement architecture.

## Gaps (verified from tree and inspected files)
1. Public graph API exposes validation only; no public immutable graph object, graph process, swap, or destroy API exists.
2. Audio source, PCM5122, TDM output and web modules are 2-line stubs and have no substantive implementation.
3. No board_config.h, protection provisioning tool, or operational firmware DSP task is present in the tracked path list despite references in policy/design; S1 means hardware-dependent implementation must stop until these owner inputs exist.
4. The sample config/schema accepts Bessel while device compiler rejects it; keep E_UNSUPPORTED until normalization convention and implementation are approved.
5. Design states 64-frame blocks but the PLAN G4 budget says p99.9 <=0.60 ms and max <=1.00 ms; 64/48k =1.333 ms. Block duration and timing gate conflict and require an ADR/human decision before timing implementation.
6. TDM, MCLK, three-DAC slot routing, I2S API version, decoder hook pin and memory capacity are proposed/unverified. Do not claim feasibility as measured.
7. Room correction is not a distinct module or measured target; the graph already has PEQ/shelf and can compose them with crossover nodes. Which filters are global vs per-band and correction source are open.

## Repository file index
See `repository_files.json` for exact tracked paths at the basis commit. The task branch commit must remain documentation-only.
