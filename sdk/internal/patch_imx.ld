/* Minimal linker script placing patch image at a fixed RAM address. */

_PATCH_BASE = DEFINED(PATCH_LOAD_ADDR) ? PATCH_LOAD_ADDR : 0x80000000;

MEMORY
{
  RAM (rxw) : ORIGIN = _PATCH_BASE, LENGTH = 512K
}

ENTRY(patch_header)

SECTIONS
{
  /* Header at the very start */
  .patch_header ORIGIN(RAM) : ALIGN(4)
  {
    KEEP(*(.patch_header))
  } > RAM

  /* Code and initialized data follow contiguously */
  .patch_image : ALIGN(4)
  {
    __patch_start = .;
    KEEP(*(.text))
    KEEP(*(.text.*))
    KEEP(*(.rodata))
    KEEP(*(.rodata.*))
    . = ALIGN(4);
    __preinit_array_start = .;
    KEEP(*(.preinit_array))
    KEEP(*(.preinit_array.*))
    __preinit_array_end = .;
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(.init_array))
    KEEP(*(.init_array.*))
    __init_array_end = .;
    . = ALIGN(4);
    __fini_array_start = .;
    KEEP(*(.fini_array))
    KEEP(*(.fini_array.*))
    __fini_array_end = .;
    . = ALIGN(4);
    __ctors_start = .;
    KEEP(*(.ctors))
    KEEP(*(.ctors.*))
    __ctors_end = .;
    . = ALIGN(4);
    __dtors_start = .;
    KEEP(*(.dtors))
    KEEP(*(.dtors.*))
    __dtors_end = .;
    KEEP(*(.data))
    KEEP(*(.data.*))
    __patch_end = .;
  } > RAM

  /* Zero-initialized data (not in the file) */
  .patch_bss (NOLOAD) : ALIGN(4)
  {
    __patch_bss_start = .;
    *(.bss*)
    *(COMMON)
    __patch_bss_end = .;
  } > RAM

  /DISCARD/ :
  {
    *(*.comment*)
    *(.note*)
  }

  /* Absolute helper symbols for C initialization (values read via &sym) */
  patch_init_addr = patch_init | 1;      /* set Thumb bit */
  patch_agent_update_buffers_addr = patch_agent_update_buffers | 1;
  patch_agent_set_buffer_addr = patch_agent_set_buffer | 1;
  patch_agent_get_buffer_size_addr = patch_agent_get_buffer_size | 1;
  patch_agent_get_param_min_addr = patch_agent_get_param_min | 1;
  patch_agent_get_param_max_addr = patch_agent_get_param_max | 1;
  patch_agent_get_param_default_addr = patch_agent_get_param_default | 1;
  patch_agent_is_param_enabled_addr = patch_agent_is_param_enabled | 1;
  patch_agent_get_param_name_addr = patch_agent_get_param_name | 1;
  patch_agent_get_param_unit_addr = patch_agent_get_param_unit | 1;
  patch_agent_set_param_addr = patch_agent_set_param | 1;
  patch_agent_special_action_addr = patch_agent_special_action | 1;
  patch_agent_get_state_idx_addr = patch_agent_get_state_idx | 1;

  /* Sizes: exclude header from image_size (header is in .patch_header) */
  patch_image_size = __patch_end - __patch_start;
  patch_bss_size = __patch_bss_end - __patch_bss_start;
}
