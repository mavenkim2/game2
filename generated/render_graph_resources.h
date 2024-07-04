namespace rendergraph
{
struct RGBlockcompress
{
	const string name = Str8Lit("RGBlockcompress");
	const u32 numResources = 2;
	const u32 numBuffers = 0;
	const u32 numTextures = 2;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource textureResources[2] = {
		{ResourceUsage::SRV, ResourceType::Texture2D, 0},
		{ResourceUsage::UAV, ResourceType::RWTexture2D, 0},
	};
	TextureView input;
	TextureView output;
};

struct RGClearIndirect
{
	const string name = Str8Lit("RGClearIndirect");
	const u32 numResources = 1;
	const u32 numBuffers = 1;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[1] = {
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
	};
	BufferView indirectCommands;
};

struct RGCullTriangle
{
	const string name = Str8Lit("RGCullTriangle");
	const u32 numResources = 3;
	const u32 numBuffers = 3;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[3] = {
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 0},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 1},
	};
	BufferView meshClusterIndices;
	BufferView indirectCommands;
	BufferView outputIndices;
	TriangleCullPushConstant push;
};

struct RGCull
{
	const string name = Str8Lit("RGCull");
	const u32 numResources = 1;
	const u32 numBuffers = 0;
	const u32 numTextures = 1;
	const ShaderParamFlags flags = ShaderParamFlags::Header;
	const PassResource textureResources[1] = {
		{ResourceUsage::SRV, ResourceType::Texture2D, 0},
	};
	TextureView depthPyramid;
};

struct RGCullCluster
{
	const string name = Str8Lit("RGCullCluster");
	const u32 numResources = 4;
	const u32 numBuffers = 4;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[4] = {
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 0},
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 1},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 1},
	};
	BufferView meshChunks;
	BufferView views;
	BufferView dispatchIndirect;
	BufferView outputMeshClusterIndex;
	ClusterCullPushConstants push;
	RGCull cull;
};

struct RGCullInstance
{
	const string name = Str8Lit("RGCullInstance");
	const u32 numResources = 6;
	const u32 numBuffers = 6;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[6] = {
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 1},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 1},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 2},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 3},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 4},
	};
	BufferView views;
	BufferView dispatchIndirectBuffer;
	BufferView meshChunks;
	BufferView occludedInstances;
	BufferView cullingStats;
	BufferView debugAABBs;
	InstanceCullPushConstants push;
	RGCull cull;
};

struct RGGenerateMips
{
	const string name = Str8Lit("RGGenerateMips");
	const u32 numResources = 2;
	const u32 numBuffers = 0;
	const u32 numTextures = 2;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource textureResources[2] = {
		{ResourceUsage::SRV, ResourceType::Texture2D, 0},
		{ResourceUsage::UAV, ResourceType::RWTexture2D, 0},
	};
	TextureView mipInSRV;
	TextureView mipOutUAV;
};

struct RGMeshHeader
{
	const string name = Str8Lit("RGMeshHeader");
	const u32 numResources = 1;
	const u32 numBuffers = 0;
	const u32 numTextures = 1;
	const ShaderParamFlags flags = ShaderParamFlags::Header;
	const PassResource textureResources[1] = {
		{ResourceUsage::SRV, ResourceType::Texture2DArray, 1},
	};
	TextureView shadowMaps;
	PushConstant push;
};

struct RGDepth
{
	const string name = Str8Lit("RGDepth");
	const u32 numResources = 0;
	const u32 numBuffers = 0;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Graphics;
	RGMeshHeader meshHeader;
};

struct RGDrawCompaction
{
	const string name = Str8Lit("RGDrawCompaction");
	const u32 numResources = 5;
	const u32 numBuffers = 5;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[5] = {
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 0},
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 1},
		{ResourceUsage::SRV, ResourceType::StructuredBuffer, 2},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 1},
	};
	BufferView indirectCommands;
	BufferView meshClusterIndices;
	BufferView dispatchIndirectBuffer;
	BufferView commandCount;
	BufferView outputIndirectCommands;
	DrawCompactionPushConstant push;
};

struct RGMesh
{
	const string name = Str8Lit("RGMesh");
	const u32 numResources = 0;
	const u32 numBuffers = 0;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Graphics;
	RGMeshHeader meshHeader;
};

struct RGDispatchPrep
{
	const string name = Str8Lit("RGDispatchPrep");
	const u32 numResources = 1;
	const u32 numBuffers = 1;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[1] = {
		{ResourceUsage::UAV, ResourceType::RWStructuredBuffer, 0},
	};
	BufferView dispatch;
	DispatchPrepPushConstant push;
};

struct RGSkinning
{
	const string name = Str8Lit("RGSkinning");
	const u32 numResources = 0;
	const u32 numBuffers = 0;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	SkinningPushConstants push;
};

}
