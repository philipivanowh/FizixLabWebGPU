#pragma once
#include "Snapshot.hpp"
#include <deque>
#include <cmath>

class Recorder {
public:
    static constexpr size_t MAX_ENTRIES  = 1000;
    static constexpr int    KEYFRAME_EVERY = 1;  // full snapshot every 10 frames

    struct Entry {
        WorldSnapshot snapshot;
        bool          isKeyframe;
    };

    void Record(const WorldSnapshot& current)
    {
        if (history.size() >= MAX_ENTRIES)
            history.pop_front();

        const bool forceKeyframe = history.empty() ||
                                   (frameCounter % KEYFRAME_EVERY == 0);

        Entry e;
        e.isKeyframe = forceKeyframe;
        e.snapshot   = forceKeyframe
                       ? current
                       : DeltaFrom(history.back().snapshot, current);

        history.push_back(e);
        frameCounter++;
    }

    bool Rewind(WorldSnapshot& out)
    {
        if (history.empty()) return false;

        // Walk back to find the nearest keyframe and reconstruct
        // Pop the current top first
        history.pop_back();

        // Now find the most recent keyframe to restore from
        for (int i = static_cast<int>(history.size()) - 1; i >= 0; i--)
        {
            if (history[i].isKeyframe) {
                out = history[i].snapshot;
                return true;
            }
        }
        return false;
    }

    bool HasHistory()  const { return !history.empty(); }
    size_t HistorySize() const { return history.size(); }
    void Clear() { history.clear(); frameCounter = 0; }

private:
    // Only store bodies that moved more than a threshold
    WorldSnapshot DeltaFrom(const WorldSnapshot& prev, const WorldSnapshot& curr)
    {
        WorldSnapshot delta;
        delta.bodies.resize(curr.bodies.size());

        for (size_t i = 0; i < curr.bodies.size(); i++)
        {
            const float posDiff = (curr.bodies[i].pos - prev.bodies[i].pos).Length();
            // If body barely moved, copy prev (cheap to store, skip interpolation cost)
            delta.bodies[i] = (posDiff > 0.01f) ? curr.bodies[i] : prev.bodies[i];
        }
        return delta;
    }

    std::deque<Entry>  history;
    int                frameCounter = 0;
};