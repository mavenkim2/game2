#include "mkRenderGraph.h"
#include "../generated/render_graph_resources.h"

namespace rendergraph
{

// TODO: IDEA: create a minimum spanning tree for each queue?

// void RenderGraph::Init()
// {
//     arena = ArenaAlloc();
// }
//
// void AddInstanceCull(RenderGraph *graph)
// {
//     PassResources instanceCullResources;
//     instanceCullResources.
//
//         graph->AddPass([&](CommandList cmd) {
//             device->BeginEvent("Instance Cull First Pass");
//             device->EndEvent("Instance Cull First Pass");
//         });
// }
//
// void RenderGraph::ExecutePasses()
// {
//    for (u32 i = 0; i < passes.size(); i++)
//     {
//         Pass *pass = &passes[i];
//         pass->func(pass->parameters);
//     }
// }
} // namespace rendergraph
