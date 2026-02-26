/* Start Header ************************************************************************/
/*!
\file       PreCompile.h
\author     Wong Jun Yu, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This reflects the brief of the PreCompile.h file.
            This file is used to include all the necessary headers for the project.
            This is to reduce the compile time of the project.
            This file is included in all the source files

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

// Standard Libraries
#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <numbers>
#include <regex>

#include <memory>
#include <bitset>
#include <set>
#include <tuple>

#include <filesystem>
#include <typeindex>

#include <shobjidl.h>
#include <windows.h>
#undef max
#undef min

// Threading and synchronization
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>


// Data types and structures
#include <vector>
#include <array>
#include <queue>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>

// Project-specific headers
#include "EngineAPI.h"
#include "Logger.h"