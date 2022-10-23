#pragma once

#ifdef _WIN32
int nanosleep(unsigned long long nsec);
#endif
#ifdef HEADER_ONLY
#include "nanosleep.cpp"
#endif