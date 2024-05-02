#ifndef SCENE_H
#define SCENE_H

#include <atomic>

namespace mk::scene
{
struct mkScene
{
    // Occlusion results
    struct OcclusionResult
    {
        u32 queryId[2];
        u32 history = ~0u;

        b32 IsOccluded() const
        {
            return history == 0;
        }
    };

    list<OcclusionResult> occlusionResults;
    // GPUBuffer queryResultBuffer[2];
    std::atomic<u32> queryCount;

    struct AABB
    {
        Rect3 bounds;
    };
    list<Rect3> aabbObjects;
};
} // namespace mk::scene

global mkScene scene;

#endif
