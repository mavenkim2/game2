#include "ShaderInterop.h"

struct InstanceDrawData
{
    uint meshIndex;
};

#define CHUNK_GROUP_SIZE 64

// NOTE: each chunk contains multiple clusters. this is mostly just for efficient usage of resources in compute shaders.
// (at least that's what I think this is for). otherwise, each thread on the instance culling shader would have to write
// all of the batches manually, (40k triangles -> ~150+ batches), which would be slow
struct MeshChunk
{
    uint numClusters;
    uint clusterOffset;
    uint meshIndex;
    uint wasVisibleLastFrame;
};

struct BatchCullPushConstants
{
    float4x4 viewProjection;
    float nearZ;
    float pyramidWidth;
    float pyramidHeight;
    float p22;
    float p23;
    uint isSecondPass;
};

struct InstanceCullPushConstants
{
    float nearZ;
    float pyramidWidth;
    float pyramidHeight;
    float p22;
    float p23;
    uint isSecondPass;
    uint numInstances;
    uint meshParamsDescriptor;
};
