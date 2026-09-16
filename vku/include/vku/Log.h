#pragma once
extern void vku_internal_printf(const char *fmt, ...);

#define VKU_PRINTF_IMPL(...)                                                   \
  vku_internal_printf(__VA_ARGS__);                                            \
  vku_internal_printf("\n")
#define VKU_LOG_INFO(...)                                                      \
  vku_internal_printf("vku::INFO: ");                                          \
  VKU_PRINTF_IMPL(__VA_ARGS__)
#define VKU_LOG_ERR(...)                                                       \
  vku_internal_printf("\n\nvku::ERROR: ");                                     \
  VKU_PRINTF_IMPL(__VA_ARGS__);                                                \
  vku_internal_printf("\n");
#define VKU_LOG_WARN(...)                                                      \
  vku_internal_printf("vku::WARN: ");                                          \
  VKU_PRINTF_IMPL(__VA_ARGS__)
