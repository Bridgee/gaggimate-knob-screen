#pragma once
#ifndef GAGGIMATE_KNOB_VALIDATION
#error Validation marker must not enter production builds
#endif
namespace knob_validation {
inline const char *imageMarker() { return "\x91" "K718-VALIDATION-ONLY" "\x92"; }
}
