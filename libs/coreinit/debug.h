#pragma once

#include <cstdio>

inline void OSFatal(const char *message) { printf("%s\n", message); };
