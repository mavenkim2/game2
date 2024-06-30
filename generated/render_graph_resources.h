namespace rendergraph
{
struct RGBlockcompress
{
	const PassResource resources[2] = {
		{67789, ResourceType::Texture2D, 0},
		{83724, ResourceType::RWTexture2D, 0},
	};
	graphics::Texture input;
	graphics::Texture output;
};

}
