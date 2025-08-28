#pragma once
/*
  ame/flecs_guard.h
  Compile-time guard for disallowed Flecs pipeline/system APIs in app code.
  This header is force-included for the game target.
  Any attempt to use these APIs in app code will fail to compile with a clear error.
*/

/* Only enforce in application code. The engine targets do not force-include this header. */
#ifndef AME_INTERNAL_ENGINE

/* Disallow explicit pipeline creation or selection in app code */
#define ecs_pipeline_init(...) AME_ERROR_ecs_pipeline_init_is_disallowed_in_app_code_use_engine_defaults
#define ecs_set_pipeline(...)  AME_ERROR_ecs_set_pipeline_is_disallowed_in_app_code_use_engine_defaults

/* Disallow importing System/Pipeline modules from app code */
#define FlecsSystemImport(...)   AME_ERROR_FlecsSystemImport_is_disallowed_in_app_code_engine_initializes_flecs
#define FlecsPipelineImport(...) AME_ERROR_FlecsPipelineImport_is_disallowed_in_app_code_engine_initializes_flecs

#endif /* AME_INTERNAL_ENGINE */

