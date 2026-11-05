#pragma once
#if defined(WOL_HOST_TEST)
#include "test_config.h"
#elif defined(WOL_USE_EXAMPLE_CONFIG)
#include "config.example.h"
#elif __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif
