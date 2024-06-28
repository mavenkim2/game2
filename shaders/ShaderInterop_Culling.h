#include "ShaderInterop.h"

#define FIRST_PASS 1
#define SECOND_PASS 2

#define CHUNK_GROUP_SIZE         64
#define INSTANCE_CULL_GROUP_SIZE 64
#define CLUSTER_CULL_GROUP_SIZE  64

#define CLUSTER_DISPATCH_OFFSET              0
#define TRIANGLE_DISPATCH_OFFSET             1
#define INSTANCE_SECOND_PASS_DISPATCH_OFFSET 2
#define NUM_DISPATCH_OFFSETS                 3

// NOTE: each chunk contains multiple clusters. this is mostly just for efficient usage of resources in compute shaders.
// (at least that's what I think this is for). otherwise, each thread on the instance culling shader would have to write
// all of the batches manually, (40k triangles -> ~150+ batches), which would be slow
struct InstanceCullPushConstants
{
    float pyramidWidth;
    float pyramidHeight;
    float nearZ;
    float farZ;
    uint numInstances;
    int meshParamsDescriptor;
    uint2 screenSize;
};

struct ClusterCullPushConstants
{
    float pyramidWidth;
    float pyramidHeight;
    float nearZ;
    float farZ;
    int meshClusterDescriptor;
    int meshParamsDescriptor;
};

struct DispatchIndirect
{
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
    uint commandCount;
};

struct TriangleCullPushConstant
{
    float4x4 worldToClip;
    int meshGeometryDescriptor;
    int meshParamsDescriptor;
    int meshClusterDescriptor;
    uint screenWidth;
    uint screenHeight;
    float nearZ;
};

struct DrawCompactionPushConstant
{
    uint drawCount;
};

struct DispatchPrepPushConstant
{
    uint index;
};
