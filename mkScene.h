#ifndef SCENE_H
#define SCENE_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkList.h"
#endif

#include <atomic>

namespace scene
{

typedef u32 Entity;                  // TODO: maybe this needs to become 64
const i32 ENTITY_MASK    = 0x3fffff; // maximum of 4 million
const i32 MATERIAL_SHIFT = 22;
const i32 MATERIAL_MASK  = 0x3ff; // max of 1024 materials
const i32 NULL_HANDLE    = 0;

struct MaterialComponent
{
    string name;
    AS_Handle textures[TextureType_Count] = {};

    V4 baseColor        = {1, 1, 1, 1};
    f32 metallicFactor  = 0.f;
    f32 roughnessFactor = 1.f;
};

template <typename Component>
class ComponentManager
{
private:
    TicketMutex mutex;
    list<Component> components;
    list<Entity> entities;
    HashIndex nameMap;
    HashIndex handleMap;

public:
    ComponentManager(i32 reserve = 0)
    {
        if (reserve != 0)
        {
            nameMap.Init(reserve, reserve);
        }
        else
        {
            nameMap.Init();
        }
        components.reserve(reserve);
        handleMap.Init();
        components.emplace_back(); // null
        mutex = {};
    }

    Component &operator[](i32 index)
    {
        return components[index];
    }

    Component *Create(string name)
    {
        Component *component = 0;
        TicketMutexScope(&mutex)
        {
            i32 hash = nameMap.Hash(name);
            nameMap.AddInHash(hash, (i32)components.size());

            components.emplace_back();
            components.back().name = name;
            for (u32 i = 0; i < ArrayLength(components.back().textures); i++)
            {
                components.back().textures[i] = {};
            }
            component = &components.back();
        }
        return component;
    }

    Component *Create(Entity entity, string name)
    {
        Assert(entity <= ENTITY_MASK);

        Component *component = 0;
        TicketMutexScope(&mutex)
        {
            i32 storedValue = ((i32)(components.size() & MATERIAL_MASK) << MATERIAL_SHIFT) |
                              ((i32)(entity & ENTITY_MASK));
            handleMap.AddInHash(entity, storedValue);
            i32 hash = nameMap.Hash(name);
            nameMap.AddInHash(hash, (i32)components.size());
            components.emplace_back();
            entities.push_back(entity);
            component = &components.back();
        }
        return component;
    }

    Component *Link(Entity entity, string name)
    {
        Component *component = 0;
        i32 index            = GetComponentIndex(name);

        if (index != -1)
        {
            TicketMutexScope(&mutex)
            {
                i32 storedValue = ((i32)(index & MATERIAL_MASK) << MATERIAL_SHIFT) |
                                  ((i32)(entity & ENTITY_MASK));
                handleMap.AddInHash(entity, index, storedValue);
                component = &components[index];
            }
        }
        return component;
    }

    b32 Remove(Entity entity)
    {
        b32 result = 0;
        TicketMutexScope(&mutex)
        {
            for (i32 i = handleMap.FirstInHash(entity); i != -1;)
            {
                u32 storedEntity   = i & ENTITY_MASK;
                u32 storedMaterial = (i >> MATERIAL_SHIFT) & MATERIAL_MASK;

                if (storedEntity == entity)
                {
                    // Remove from entity lookup table
                    handleMap.indexChain[i] = handleMap.indexChain[storedMaterial];

                    // Remove from name lookup table
                    i32 hash = nameMap.Hash(components[storedMaterial].name);
                    nameMap.RemoveFromHash(hash, (i32)storedMaterial);

                    // Swap with end of list then pop
                    components[storedMaterial] = std::move(components.back()); // move instead of copy
                    entities[storedMaterial]   = entities.back();
                    components.pop_back();
                    entities.pop_back();
                    result = 1;
                    break;
                }
                i = handleMap.NextInHash(storedMaterial);
            }
        }
        return result;
    }

    Component *GetComponent(Entity entity)
    {
        Component *component = 0;
        TicketMutexScope(&mutex)
        {
            for (i32 i = handleMap.FirstInHash(entity); i != -1;)
            {
                u32 storedEntity   = i & ENTITY_MASK;
                u32 storedMaterial = (i >> MATERIAL_SHIFT) & MATERIAL_MASK;
                if (storedEntity == entity)
                {
                    component = &components[storedMaterial];
                    break;
                }

                i = handleMap.NextInHash(storedMaterial);
            }
        }
        return component;
    }

    i32 GetComponentIndex(string name)
    {
        i32 hash = nameMap.Hash(name);

        i32 index = -1;
        TicketMutexScope(&mutex)
        {
            for (i32 i = nameMap.FirstInHash(hash); i != -1; i = nameMap.NextInHash(i))
            {
                if (components[i].name == name)
                {
                    index = i;
                    break;
                }
            }
        }
        return index;
    }

    Component *GetComponent(string name)
    {
        i32 hash             = nameMap.Hash(name);
        Component *component = 0;

        TicketMutexScope(&mutex)
        {
            for (i32 i = nameMap.FirstInHash(hash); i != -1; i = nameMap.NextInHash(i))
            {
                if (components[i].name == name)
                {
                    component = &components[i];
                    break;
                }
            }
        }
        return component;
    }

    // ComponentHandle CreateComponent()
    // {
    //     static atomic<u32> componentGenerator = NULL_HANDLE + 1;
    //     return componentGenerator.fetch_add(1);
    // }
};

struct Scene
{
    struct SceneGraphNode
    {
        SceneGraphNode *parent;
        SceneGraphNode *first;
        SceneGraphNode *last;
        SceneGraphNode *next;
        SceneGraphNode *prev;

        Entity entity;
    };

    ComponentManager<MaterialComponent> materials;

    Entity CreateEntity()
    {
        static std::atomic<u32> entityGen = NULL_HANDLE + 1;
        return entityGen.fetch_add(1);
    }
    // Occlusion results
    // struct OcclusionResult
    // {
    //     u32 queryId[2];
    //     u32 history = ~0u;
    //
    //     b32 IsOccluded() const
    //     {
    //         return history == 0;
    //     }
    // };
    //
    // list<OcclusionResult> occlusionResults;
    // // GPUBuffer queryResultBuffer[2];
    // std::atomic<u32> queryCount;
    //
    // struct AABB
    // {
    //     Rect3 bounds;
    // };
    // list<Rect3> aabbObjects;
};

} // namespace scene

global scene::Scene gameScene;
#endif
