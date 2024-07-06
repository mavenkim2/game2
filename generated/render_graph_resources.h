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
		{graphics::ResourceViewType::SRV, HLSLType::Texture2D, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWTexture2D, 0},
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
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
	};
	BufferView indirectCommands;
};

struct RGCull
{
	const string name = Str8Lit("RGCull");
	const u32 numResources = 1;
	const u32 numBuffers = 0;
	const u32 numTextures = 1;
	const ShaderParamFlags flags = ShaderParamFlags::Header;
	const PassResource textureResources[1] = {
		{graphics::ResourceViewType::SRV, HLSLType::Texture2D, 0},
	};
	TextureView depthPyramid;
};

struct RGCullInstance
{
	const string name = Str8Lit("RGCullInstance");
	const u32 numResources = 6;
	const u32 numBuffers = 6;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[6] = {
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 1},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 1},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 2},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 3},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 4},
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

struct RGMeshHeader
{
	const string name = Str8Lit("RGMeshHeader");
	const u32 numResources = 1;
	const u32 numBuffers = 0;
	const u32 numTextures = 1;
	const ShaderParamFlags flags = ShaderParamFlags::Header;
	const PassResource textureResources[1] = {
		{graphics::ResourceViewType::SRV, HLSLType::Texture2DArray, 1},
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

struct RGGenerateMips
{
	const string name = Str8Lit("RGGenerateMips");
	const u32 numResources = 2;
	const u32 numBuffers = 0;
	const u32 numTextures = 2;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource textureResources[2] = {
		{graphics::ResourceViewType::SRV, HLSLType::Texture2D, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWTexture2D, 0},
	};
	TextureView mipInSRV;
	TextureView mipOutUAV;
};

struct RGCullCluster
{
	const string name = Str8Lit("RGCullCluster");
	const u32 numResources = 4;
	const u32 numBuffers = 4;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[4] = {
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 0},
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 1},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 1},
	};
	BufferView meshChunks;
	BufferView views;
	BufferView dispatchIndirect;
	BufferView outputMeshClusterIndex;
	ClusterCullPushConstants push;
	RGCull cull;
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

struct RGDrawCompaction
{
	const string name = Str8Lit("RGDrawCompaction");
	const u32 numResources = 5;
	const u32 numBuffers = 5;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[5] = {
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 0},
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 1},
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 2},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 1},
	};
	BufferView indirectCommands;
	BufferView meshClusterIndices;
	BufferView dispatchIndirectBuffer;
	BufferView commandCount;
	BufferView outputIndirectCommands;
	DrawCompactionPushConstant push;
};

struct RGCullTriangle
{
	const string name = Str8Lit("RGCullTriangle");
	const u32 numResources = 3;
	const u32 numBuffers = 3;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[3] = {
		{graphics::ResourceViewType::SRV, HLSLType::StructuredBuffer, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 1},
	};
	BufferView meshClusterIndices;
	BufferView indirectCommands;
	BufferView outputIndices;
	TriangleCullPushConstant push;
};

struct RGDispatchPrep
{
	const string name = Str8Lit("RGDispatchPrep");
	const u32 numResources = 1;
	const u32 numBuffers = 1;
	const u32 numTextures = 0;
	const ShaderParamFlags flags = ShaderParamFlags::Compute;
	const PassResource bufferResources[1] = {
		{graphics::ResourceViewType::UAV, HLSLType::RWStructuredBuffer, 0},
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
