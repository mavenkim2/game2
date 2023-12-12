#ifndef KEEPMOVINGFORWAD_INTRINSIC_H
#include "keepmovingforward_types.h"

inline uint32 RoundFloatToUint32(float value) { return (uint32)(value + 0.5f); }
inline uint32 RoundFloatToInt32(float value) { return (int32)(value + 0.5f); }

#define KEEPMOVINGFORWAD_INTRINSIC_H
#endif
