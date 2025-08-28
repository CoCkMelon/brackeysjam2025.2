#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Initialize and cache executable base path once.
// Safe to call multiple times; subsequent calls are no-ops.
void pathutil_init(void);

// Returns cached base path (may be empty string if unavailable).
// This pointer remains valid for the lifetime of the process.
const char* pathutil_base(void);

#ifdef __cplusplus
}
#endif
