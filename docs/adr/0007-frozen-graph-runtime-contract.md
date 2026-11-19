# ADR 0007: Frozen graph build and runtime ownership contract
- Status: Accepted for planning; public header change requires CODEOWNERS review
- Date: 2026-11-19
- Supersedes proposed graph signatures in docs/api.md §2.2 and docs/engineering/02_contracts_and_verification.md

## Decision
Keep dspg_validate(json,len,report) unchanged. There is one opaque build API:
```c
typedef struct dspg dspg_t;
typedef struct { void *memory; size_t size; } dspg_arena_t;
typedef struct { uint16_t output_mask; uint16_t max_frames; size_t required_bytes; } dspg_info_t;
int dspg_build(const char *json, size_t len, const dspg_protect_t *protect,
               size_t n_protect, dspg_arena_t arena, dspg_t **out,
               dspg_info_t *info, dspg_report_t *report);
int dspg_process(dspg_t *g, const float *const input[2],
                 float *const output[8], uint32_t n_frames);
void dspg_reset(dspg_t *g);
```
Builder is core-0-only; sets *out=NULL on every failure; never mutates a published graph. Caller supplies max_align_t-aligned memory. Required bytes are reported exactly; insufficient capacity fails before publication. Graph, coefficients and state fit wholly in arena; hidden allocation is forbidden. Build always validates protection; there is no bypass flag.
Process is DSP-owner-only, allocation-free and bounded, frames 1..64. Inputs/outputs are planar, 2 input and 8 output pointers; unused output pointers may be NULL, active output-mask pointers may not. Aliasing is forbidden. On non-finite output return -1 and zero every provided output buffer for the requested frames. Arena lifetime remains caller-owned; reclaim only after DSP owner acknowledges generation switch. No free function.
Stable topological order breaks ties by original node array order. Mix input order is JSON edge-array order and float32 accumulation is left-to-right. Each stateful node owns one state per audio channel that reaches it and is processed once per block; fan-out reuses the computed block. ADR 0011 supersedes any earlier per-output-path reading of this rule. No implicit routing, polarity, delay, or channel duplication.

## Rejected
Return-pointer allocation API hides ownership; unspecified allocator options permit interpretation; immediate free after swap races in-flight blocks.

## Required before implementation
A big-model-owned test-only commit must land arena-size vectors and process golden vectors first. Host core uses caller arena; target integration must source it from internal SRAM. Any mismatch is an escalation, not a 7B design choice.

## Verification status
Planning contract only; no code or runtime behavior verified.
