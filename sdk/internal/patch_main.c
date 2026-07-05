#include "PatchABI.h"
#include "PatchCppWrapper.h"

#include <stdint.h>

extern void __libc_init_array(void);
void _init(void) {}

void* __dso_handle = NULL;

// Linker symbols from patch_imx.ld
extern const uint8_t __patch_start;
extern const uint8_t __patch_end;
extern const uint8_t __patch_bss_start;
extern const uint8_t __patch_bss_end;

// Absolute value symbols (exposed by linker; read value via &symbol)
extern const uintptr_t patch_init_addr;
extern const uintptr_t patch_agent_update_buffers_addr;
extern const uintptr_t patch_agent_set_buffer_addr;
extern const uintptr_t patch_agent_get_buffer_size_addr;
extern const uintptr_t patch_agent_get_param_min_addr;
extern const uintptr_t patch_agent_get_param_max_addr;
extern const uintptr_t patch_agent_get_param_default_addr;
extern const uintptr_t patch_agent_is_param_enabled_addr;
extern const uintptr_t patch_agent_get_param_name_addr;
extern const uintptr_t patch_agent_get_param_unit_addr;
extern const uintptr_t patch_agent_set_param_addr;
extern const uintptr_t patch_agent_get_state_idx_addr;
extern const uintptr_t patch_image_size;
extern const uintptr_t patch_bss_size;
extern const uintptr_t patch_agent_special_action_addr;

void patch_init(const PatchEnv* env)
{

    if (env == NULL || env->abi_version != PATCH_ABI_VERSION)
    {
        return;
    }
    static int ctors_initialized = 0;
    if (!ctors_initialized)
    {
        __libc_init_array();
        ctors_initialized = 1;
    }
    patch_agent_init(env);
}

// Header placed at start of binary in a dedicated section.
// Stores absolute addresses; the toolchain sets the Thumb bit automatically
// when taking function addresses of Thumb code, but we use linker helpers for
__attribute__((section(".patch_header"))) const PatchHeader patch_header = {
    .magic = PATCH_MAGIC,
    .abi_version = PATCH_ABI_VERSION,
    .flags = PATCH_FLAG_NONE,
    .init = (uintptr_t)&patch_init_addr,
    .agent_update_buffers = (PatchAgentUpdateBuffersFn)(uintptr_t)&patch_agent_update_buffers_addr,
    .agent_set_buffer = (PatchAgentSetBufferFn)(uintptr_t)&patch_agent_set_buffer_addr,
    .agent_get_buffer_size =
      (PatchAgentGetBufferSizeFn)(uintptr_t)&patch_agent_get_buffer_size_addr,
    .agent_get_param_min = (PatchAgentGetParamScalarFn)(uintptr_t)&patch_agent_get_param_min_addr,
    .agent_get_param_max = (PatchAgentGetParamScalarFn)(uintptr_t)&patch_agent_get_param_max_addr,
    .agent_get_param_default =
      (PatchAgentGetParamScalarFn)(uintptr_t)&patch_agent_get_param_default_addr,
    .agent_is_param_enabled =
      (PatchAgentIsParamEnabledFn)(uintptr_t)&patch_agent_is_param_enabled_addr,
    .agent_get_param_name = (PatchAgentGetParamStringFn)(uintptr_t)&patch_agent_get_param_name_addr,
    .agent_get_param_unit = (PatchAgentGetParamStringFn)(uintptr_t)&patch_agent_get_param_unit_addr,
    .agent_set_param = (PatchAgentSetParamFn)(uintptr_t)&patch_agent_set_param_addr,
    .agent_special_action = (PatchAgentSpecialActionFn)(uintptr_t)&patch_agent_special_action_addr,
    .agent_get_state_idx = (PatchAgentGetStateIdxFn)(uintptr_t)&patch_agent_get_state_idx_addr,
    .image_size = (uint32_t)(uintptr_t)&patch_image_size,
    .bss_size = (uint32_t)(uintptr_t)&patch_bss_size,
    .bss_begin = (uintptr_t)&__patch_bss_start,
    .reserved = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

// Variable symbol name 'patch_header' matches ENTRY in the linker script
