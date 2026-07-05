#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple, C-compatible ABI between the main firmware and patch binaries.
// The patch binary is linked to a fixed RAM address and exported as a raw
// .bin that starts with PatchHeader followed by code/data sections.

// Magic value: 'PTCH'
#define PATCH_MAGIC 0x48435450u

// ABI version. Bump when making breaking changes to structures.
#define PATCH_ABI_VERSION 0x000Bu

// Optional flags for the image (reserved for future use)
#define PATCH_FLAG_NONE 0x0000u

// Forward declaration and function pointer types used by the loader and
// patches.
typedef struct PatchEnv PatchEnv;
typedef void (*PatchInitFn)(const PatchEnv *env);

// Agent control hooks exposed by patch images. All functions are optional but
// recommended for full integration with InjectionWrapper.
typedef float (*PatchAgentUpdateBuffersFn)(const PatchEnv *env, float *left,
                                           float *right, int samples);
typedef void (*PatchAgentSetBufferFn)(const PatchEnv *env, float *buffer);
typedef int (*PatchAgentGetBufferSizeFn)(const PatchEnv *env);
typedef float (*PatchAgentGetParamScalarFn)(const PatchEnv *env, int idx);
typedef uint8_t (*PatchAgentIsParamEnabledFn)(const PatchEnv *env, int idx,
                                              int sourceId);
typedef void (*PatchAgentGetParamStringFn)(const PatchEnv *env, int idx,
                                           char *out, size_t out_len);
typedef void (*PatchAgentSetParamFn)(const PatchEnv *env, int idx, float value);
typedef void (*PatchAgentSpecialActionFn)(const PatchEnv *env, int idx);
typedef int (*PatchAgentGetStateIdxFn)(const PatchEnv *env);

// Environment provided by the main firmware to the patch at init/update time.
// Keep this minimal and stable. Extend by adding fields at the end.
struct PatchEnv {
  uint32_t abi_version; // must equal PATCH_ABI_VERSION
  void *user_ctx;
};

// Header placed at the very beginning of the patch binary in RAM.
// The init/update fields are absolute addresses (with Thumb bit set on ARM).
typedef struct PatchHeader {
  uint32_t magic;       // PATCH_MAGIC
  uint16_t abi_version; // PATCH_ABI_VERSION
  uint16_t flags;       // PATCH_FLAG_*

  uintptr_t init; // absolute address of init(const PatchEnv*)

  PatchAgentUpdateBuffersFn agent_update_buffers;     // may be NULL
  PatchAgentSetBufferFn agent_set_buffer;             // may be NULL
  PatchAgentGetBufferSizeFn agent_get_buffer_size;    // may be NULL
  PatchAgentGetParamScalarFn agent_get_param_min;     // may be NULL
  PatchAgentGetParamScalarFn agent_get_param_max;     // may be NULL
  PatchAgentGetParamScalarFn agent_get_param_default; // may be NULL
  PatchAgentIsParamEnabledFn agent_is_param_enabled;  // may be NULL
  PatchAgentGetParamStringFn agent_get_param_name;    // may be NULL
  PatchAgentGetParamStringFn agent_get_param_unit;    // may be NULL
  PatchAgentSetParamFn agent_set_param;               // may be NULL
  PatchAgentSpecialActionFn agent_special_action;     // may be NULL
  PatchAgentGetStateIdxFn agent_get_state_idx;        // may be NULL
  uint32_t image_size; // size of the file to load into RAM starting at header
                       // (excludes BSS)
  uintptr_t bss_begin; // pointer to the beginning of bss section
  uint32_t bss_size;   // size of zero-initial data to reserve/clear after image
  uint32_t reserved[16]; // set to 0
} PatchHeader;

#ifdef __cplusplus
} // extern "C"
#endif
