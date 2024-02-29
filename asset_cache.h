struct AS_Node;
struct AS_Slot;
internal void AS_EntryPoint(void *p);
JOB_CALLBACK(AS_LoadAsset);

struct AS_CacheState
{
    Arena *arena;

    // Must be power of 2
    u8 *ringBuffer;
    u64 ringBufferSize;
    u64 readPos;
    u64 writePos;

    TicketMutex mutex;

    // Threads
    OS_Handle *threads;
    u32 threadCount;
    OS_Handle hotloadThread;

    OS_Handle writeSemaphore;
    OS_Handle readSemaphore;

    // Hash table for assets
    u32 numSlots;
    AS_Slot *assetSlots;

    AS_Node *freeNode;
};

enum AS_Type
{
    AS_Null,
    AS_Mesh,
    AS_Texture,
    AS_Skeleton,
    AS_Model,
    AS_Shader,
    AS_GLTF,
    AS_Count,
};

struct AS_Node
{
    // TODO: some sort of memory management scheme for this
    Arena *arena;
    u64 hash;
    u64 lastModified;
    string path;
    string data;
    AS_Type type;

    union
    {
        SkeletonHelp skeleton;
        Texture texture;
    };

    AS_Node *next;
};

struct AS_Slot
{
    AS_Node *first;
    AS_Node *last;

    Mutex mutex;
};
