// Preserve the half-second target used by the knob without rounding it to a whole second.
export const formatGrindTargetTime = milliseconds => `${Math.floor(milliseconds / 100) / 10}s`;
