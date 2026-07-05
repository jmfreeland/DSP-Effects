#ifndef PATCH_CPP_WRAPPER_H
#define PATCH_CPP_WRAPPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct PatchEnv;
    void patch_agent_set_buffer(const PatchEnv* env, float* buffer);
    int patch_agent_get_buffer_size(const PatchEnv* env);
    float patch_agent_update_buffers(const PatchEnv* env, float* left, float* right, int samples);
    float patch_agent_get_param_min(const PatchEnv* env, int idx);
    float patch_agent_get_param_max(const PatchEnv* env, int idx);
    float patch_agent_get_param_default(const PatchEnv* env, int idx);
    uint8_t patch_agent_is_param_enabled(const PatchEnv* env, int idx, int sourceId);
    void patch_agent_get_param_name(const PatchEnv* env, int idx, char* out, size_t bufferSize);
    void patch_agent_get_param_unit(const PatchEnv* env, int idx, char* out, size_t bufferSize);
    int patch_agent_get_state_idx(const PatchEnv* env);
    void patch_agent_set_param(const PatchEnv* env, int idx, float value);
    void patch_agent_special_action(const PatchEnv* env, int idx);
    void patch_agent_init(const PatchEnv* env);

#ifdef __cplusplus
}
#endif

#endif // PATCH_CPP_WRAPPER_H
