#pragma once
#include "vku/Log.h"

#define VK_CHECK(X) {int _lineNumber = __LINE__; const char* _filePath = __FILE__;\
if(X != VK_SUCCESS){\
VKU_LOG_ERR("VK check failed at %s Line %d: %s", _filePath, _lineNumber, #X);}}

static constexpr int        MAX_FRAMES_IN_FLIGHT = 2;
