namespace rendergraph
{
struct RGCull
{
	const string name = Str8Lit("RGCull");
	const PassResource resources[1] = {
		{2658854520, ResourceType::Texture2D, 0},
	};
	graphics::Texture depthPyramid;
};

struct RGGlobals
{
	const string name = Str8Lit("RGGlobals");
	const PassResource resources[5] = {
		{2661457772, ResourceType::StructuredBuffer, 50},
		{2661473902, ResourceType::StructuredBuffer, 51},
		{2661462225, ResourceType::StructuredBuffer, 52},
		{2661479358, ResourceType::StructuredBuffer, 53},
		{2661505222, ResourceType::StructuredBuffer, 54},
	};
	graphics::GPUBuffer samplerLinearWrap;
	graphics::GPUBuffer samplerNearestWrap;
	graphics::GPUBuffer samplerLinearClamp;
	graphics::GPUBuffer samplerNearestClamp;
	graphics::GPUBuffer samplerShadowMap;
};

struct RGBlockcompress
{
	const string name = Str8Lit("RGBlockcompress");
	const PassResource resources[2] = {
		{2666927219, ResourceType::Texture2D, 0},
		{2667042352, ResourceType::RWTexture2D, 0},
	};
	graphics::Texture input;
	graphics::Texture output;
	RGGlobals globals;
};

struct RGGenerateMips
{
	const string name = Str8Lit("RGGenerateMips");
	const PassResource resources[2] = {
		{2665542537, ResourceType::Texture2D, 0},
		{2665491337, ResourceType::RWTexture2D, 0},
	};
	graphics::Texture mipInSRV;
	graphics::Texture mipOutUAV;
	RGGlobals globals;
};

struct RGCullTriangle
{
	const string name = Str8Lit("RGCullTriangle");
	const PassResource resources[3] = {
		{2665879977, ResourceType::StructuredBuffer, 0},
		{2665933906, ResourceType::RWStructuredBuffer, 0},
		{2665553398, ResourceType::RWStructuredBuffer, 1},
	};
	graphics::GPUBuffer meshClusterIndices;
	graphics::GPUBuffer indirectCommands;
	graphics::GPUBuffer outputIndices;
	RGGlobals globals;
};

struct RGCullInstance
{
	const string name = Str8Lit("RGCullInstance");
	const PassResource resources[6] = {
		{2665521987, ResourceType::StructuredBuffer, 1},
		{2665992211, ResourceType::RWStructuredBuffer, 0},
		{2665501168, ResourceType::RWStructuredBuffer, 1},
		{2665923702, ResourceType::RWStructuredBuffer, 2},
		{2665596450, ResourceType::RWStructuredBuffer, 3},
		{2665481457, ResourceType::RWStructuredBuffer, 4},
	};
	graphics::GPUBuffer views;
	graphics::GPUBuffer dispatchIndirectBuffer;
	graphics::GPUBuffer meshChunks;
	graphics::GPUBuffer occludedInstances;
	graphics::GPUBuffer cullingStats;
	graphics::GPUBuffer debugAABBs;
	RGGlobals globals;
	RGCull cull;
};

struct RGMeshHeader
{
	const string name = Str8Lit("RGMeshHeader");
	const PassResource resources[1] = {
		{2663807856, ResourceType::Texture2DArray, 1},
	};
	RGGlobals globals;
};

struct RGMesh
{
	const string name = Str8Lit("RGMesh");
	RGMeshHeader meshHeader;
};

struct RGDispatchPrep
{
	const string name = Str8Lit("RGDispatchPrep");
	const PassResource resources[1] = {
		{2665508678, ResourceType::RWStructuredBuffer, 0},
	};
	graphics::GPUBuffer dispatch;
	RGGlobals globals;
};

struct RGDepth
{
	const string name = Str8Lit("RGDepth");
	RGMeshHeader meshHeader;
};

struct RGCullCluster
{
	const string name = Str8Lit("RGCullCluster");
	const PassResource resources[4] = {
		{2665106422, ResourceType::StructuredBuffer, 0},
		{2664773733, ResourceType::StructuredBuffer, 1},
		{2665154780, ResourceType::RWStructuredBuffer, 0},
		{2664999176, ResourceType::RWStructuredBuffer, 1},
	};
	graphics::GPUBuffer meshChunks;
	graphics::GPUBuffer views;
	graphics::GPUBuffer dispatchIndirect;
	graphics::GPUBuffer outputMeshClusterIndex;
	RGGlobals globals;
	RGCull cull;
};

struct RGSkinning
{
	const string name = Str8Lit("RGSkinning");
	RGGlobals globals;
};

struct RGDrawCompaction
{
	const string name = Str8Lit("RGDrawCompaction");
	const PassResource resources[5] = {
		{2667790792, ResourceType::StructuredBuffer, 0},
		{2667818701, ResourceType::StructuredBuffer, 1},
		{2667742757, ResourceType::StructuredBuffer, 2},
		{2667343391, ResourceType::RWStructuredBuffer, 0},
		{2667749956, ResourceType::RWStructuredBuffer, 1},
	};
	graphics::GPUBuffer indirectCommands;
	graphics::GPUBuffer meshClusterIndices;
	graphics::GPUBuffer dispatchIndirectBuffer;
	graphics::GPUBuffer commandCount;
	graphics::GPUBuffer outputIndirectCommands;
	RGGlobals globals;
};

struct RGClearIndirect
{
	const string name = Str8Lit("RGClearIndirect");
	const PassResource resources[1] = {
		{2666378894, ResourceType::RWStructuredBuffer, 0},
	};
	graphics::GPUBuffer indirectCommands;
	RGGlobals globals;
};

}
