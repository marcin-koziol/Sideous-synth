/*
 * Sideous - tempo-synced rate divisions, shared by the LFO and arpeggiator.
 */

#pragma once

namespace sideous {

// note-length divisions for tempo-synced rates; "beats" means quarter-notes,
// matching standard BPM convention.
enum class SyncDivision {
    Whole = 0, HalfDotted, Half, HalfTriplet,
    QuarterDotted, Quarter, QuarterTriplet,
    EighthDotted, Eighth, EighthTriplet,
    SixteenthDotted, Sixteenth, SixteenthTriplet,
    ThirtySecond
};

inline float syncDivisionBeats(SyncDivision d) noexcept
{
    switch (d)
    {
    case SyncDivision::Whole:            return 4.0f;
    case SyncDivision::HalfDotted:       return 3.0f;
    case SyncDivision::Half:             return 2.0f;
    case SyncDivision::HalfTriplet:      return 4.0f / 3.0f;
    case SyncDivision::QuarterDotted:    return 1.5f;
    case SyncDivision::Quarter:          return 1.0f;
    case SyncDivision::QuarterTriplet:   return 2.0f / 3.0f;
    case SyncDivision::EighthDotted:     return 0.75f;
    case SyncDivision::Eighth:           return 0.5f;
    case SyncDivision::EighthTriplet:    return 1.0f / 3.0f;
    case SyncDivision::SixteenthDotted:  return 0.375f;
    case SyncDivision::Sixteenth:        return 0.25f;
    case SyncDivision::SixteenthTriplet: return 1.0f / 6.0f;
    case SyncDivision::ThirtySecond:
    default:                             return 0.125f;
    }
}

inline float hzFromSyncDivision(SyncDivision d, double bpm) noexcept
{
    const float beats = syncDivisionBeats(d);
    return (float)(bpm / (60.0 * (double)beats));
}

} // namespace sideous
