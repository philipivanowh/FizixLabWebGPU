#pragma once

#include "Snapshot.hpp"
#include <deque>
#include <cmath>
#include <cstddef>

// ================================================================
// Recorder
//
// Stores a rolling history of WorldSnapshots for rewind / scrub.
//
// Two modes of reading back:
//   • Random access  — GetFrame(index)   used by the timeline scrubber
//   • Sequential pop — Rewind(out)       used by the hold-to-rewind button
//
// Memory cap: MAX_ENTRIES frames.  When full, the oldest frame is
// silently dropped from the front (sliding window).
// ================================================================
class Recorder
{
public:
    // ── Configuration ────────────────────────────────────────────
    static constexpr size_t MAX_ENTRIES = 10000; // max frames kept in memory
    static constexpr int KEYFRAME_EVERY = 1;     // 1 = every frame is a keyframe
                                                 // N = full snapshot every N frames,
                                                 //     deltas in between

    // ── Internal storage ─────────────────────────────────────────
    struct Entry
    {
        WorldSnapshot snapshot;
        bool isKeyframe = true;
        float simulationTimeMs = 0.0f;
    };

    // ── Record ───────────────────────────────────────────────────
    // Call once per tick (or every recordInterval ticks from Engine).
    // Automatically decides keyframe vs delta based on KEYFRAME_EVERY.
    void Record(const WorldSnapshot &current, float simTimeMs)
    {
        // Drop oldest when full
        if (history.size() >= MAX_ENTRIES)
            history.pop_front();

        const bool forceKeyframe =
            history.empty() ||
            (frameCounter % KEYFRAME_EVERY == 0);

        Entry e;
        e.isKeyframe = forceKeyframe;
        e.snapshot = forceKeyframe
                         ? current
                         : DeltaFrom(history.back().snapshot, current);

        e.simulationTimeMs = simTimeMs;
        history.push_back(e);
        frameCounter++;
    }

    // ── Query sim time at a recorded frame ────────────────────────
    float GetFrameTime(size_t index) const
    {
        if (index >= history.size())
            return 0.0f;
        return history[index].simulationTimeMs;
    }

    // ── Random access (timeline scrubber) ────────────────────────
    // Returns nullptr if index is out of range.
    const WorldSnapshot *GetFrame(size_t index) const
    {
        if (index >= history.size())
            return nullptr;
        return &history[index].snapshot;
    }

    // ── Truncate (resume from scrub position) ────────────────────
    // Discards every entry after `index` so that when the simulation
    // resumes from a scrubbed position, the future frames are gone
    // and history won't diverge.
    void TruncateAfter(size_t index)
    {
        if (index + 1 < history.size())
            history.erase(history.begin() + static_cast<long>(index) + 1,
                          history.end());
    }

    // ── Sequential rewind (hold-button) ──────────────────────────
    // Pops the most recent frame into `out`.
    // Returns false when history is exhausted.
    bool Rewind(WorldSnapshot &out)
    {
        if (history.empty())
            return false;
        out = history.back().snapshot;
        history.pop_back();
        return true;
    }

    // ── Queries ───────────────────────────────────────────────────
    bool HasHistory() const { return !history.empty(); }
    size_t HistorySize() const { return history.size(); }

    void Clear()
    {
        history.clear();
        frameCounter = 0;
    }

private:
    // ── Delta encoding ────────────────────────────────────────────
    // For non-keyframes: bodies that barely moved reuse the previous
    // frame's data, saving memory and copy cost for static objects.
    WorldSnapshot DeltaFrom(const WorldSnapshot &prev,
                            const WorldSnapshot &curr)
    {
        WorldSnapshot delta;
        delta.bodies.resize(curr.bodies.size());

        for (size_t i = 0; i < curr.bodies.size(); i++)
        {
            const float posDiff =
                (curr.bodies[i].pos - prev.bodies[i].pos).Length();

            // Threshold: only store new data if the body actually moved
            delta.bodies[i] = (posDiff > 0.01f)
                                  ? curr.bodies[i]
                                  : prev.bodies[i];
        }
        return delta;
    }

    std::deque<Entry> history;
    int frameCounter = 0;
};