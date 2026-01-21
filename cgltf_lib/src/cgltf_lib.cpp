// myextension.cpp
// Extension lib defines
#define LIB_NAME "cgltf_lib"
#define MODULE_NAME "cgltf"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <dmsdk/gamesys/components/comp_collection_proxy.h>
#include <dmsdk/gamesys/components/comp_factory.h>
#include <dmsdk/dlib/array.h>

#define CGLTF_EXPORT extern

/* Expose api for use by external */
#include "cgltf/cgltf.h"
#include "cgltf/cgltf_write.h"

/* cgltf files to make a simple cgltf lib */
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf/cgltf_write.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <map>
#include <string>
#include <fstream>

static std::map<std::string, cgltf_data*>     loaded_files;

struct GameCtx
{
    dmResource::HFactory        m_Factory;
    dmConfigFile::HConfig       m_ConfigFile;

    dmGameObject::HCollection   m_MainCollection;
    dmGameObject::HCollection   m_LevelCollection;

    dmGameSystem::HFactoryWorld         m_FactoryWorld;
    dmGameSystem::HFactoryComponent     m_MeshFactory;

    dmArray<dmGameObject::HInstance>    m_Meshes;

    uint64_t    m_LastFrameTime;
    float       m_SpawnTimer;
    float       m_SpawnTimerInterval;
    
    //GameState   m_State;
    //int         m_SubState;

    bool    m_IsInitialized;
    bool    m_HasError;
    bool    m_LevelInitialized;

    GameCtx()
    {
        memset(this, 0, sizeof(*this));
    }
};

static GameCtx* g_Ctx = nullptr;

const dmBuffer::StreamDeclaration streams_standard_decl[] = {
    {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_UINT16, 2},
    {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
};

const dmBuffer::StreamDeclaration streams_notex_decl[] = {
    {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
};

const dmBuffer::StreamDeclaration streams_normals_decl[] = {
    {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("normal"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_UINT16, 2},
    {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
};

static void CreatePrimitive(dmGameObject::HInstance inst, cgltf_float* data, int stream_type, int stream_size )
{
//     local res_path = go.get("#mesh", "vertices")
//     local buf = resource.get_buffer(res_path)
// 
//     uint32_t component_type_index;
//     dmGameObject::HComponent component;
//     dmGameObject::HComponentWorld world;
//     dmGameObject::Result r = dmGameObject::GetComponent(inst, dmHashString64("mesh"), &component_type_index, &component, &world);
//     assert(dmGameObject::RESULT_OK == r);

    dmBuffer::HBuffer buffer = 0x0;
    int stream_count = 3;
    dmBuffer::Result r;
    switch(stream_type) {
        case 1: 
            stream_count = 3;
            r = dmBuffer::Create(stream_size, streams_standard_decl, stream_count, &buffer);
            break;
        case 2: 
            stream_count = 3;
            r = dmBuffer::Create(stream_size, streams_notex_decl, stream_count, &buffer);
            break;
        case 3: 
            stream_count = 3;
            r = dmBuffer::Create(stream_size, streams_normals_decl, stream_count, &buffer);
            break;
        default:
            stream_count = 3;
            r = dmBuffer::Create(stream_size, streams_standard_decl, stream_count, &buffer);
            break;        
    }

    if (r != dmBuffer::RESULT_OK) {
        // handle error
    }    

    cgltf_float* bytes = 0x0;
    uint32_t size = 0;
    uint32_t count = 0;
    uint32_t components = 0;
    uint32_t stride = 0;    

    r = dmBuffer::GetStream(buffer, dmHashString64("position"), (void**)&bytes, &count, &components, &stride);    
    size_t idx = 0;
    if (r == dmBuffer::RESULT_OK) {
        for (int i = 0; i < count; ++i)
        {
            for (int c = 0; c < components; ++c)
            {
                bytes[c] = data[idx++];
            }
            bytes += stride;
        }
    } else {
        // handle error
    }
        
    r = dmBuffer::ValidateBuffer(buffer);    
}

static dmGameObject::HInstance SpawnMesh(dmGameSystem::HFactoryComponent factory, const cgltf_float _pos[3], 
                                const cgltf_float _rot[4], const cgltf_float _scl[3])
{
    GameCtx* ctx = g_Ctx;

    if (ctx->m_Meshes.Full())
    {
        dmLogWarning("Mesh buffer is full, skipping spawn of new mesh.");
        return nullptr;
    }

    dmVMath::Point3 position(_pos[0], _pos[1], _pos[2]);
    dmVMath::Quat rotation(_rot[0], _rot[1], _rot[2], _rot[3]);
    dmVMath::Vector3 scale(_scl[0], _scl[1], _scl[2]);

    // dmVMath::Point3 position(0,0,0);
    // dmVMath::Quat rotation(0,0,0,1);
    // dmVMath::Vector3 scale(2, 2, 2);

    dmhash_t meshid = dmGameObject::CreateInstanceId();
    dmGameObject::HPropertyContainer properties = 0;

    dmGameObject::HInstance instance = 0;
    dmGameObject::Result result = dmGameSystem::CompFactorySpawn(ctx->m_FactoryWorld, factory, ctx->m_MainCollection,
                                                                 meshid, position, rotation, scale, properties, &instance);

    ctx->m_Meshes.Push(instance);
    return instance;
}

std::string readFile(const std::string &fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    std::ifstream::pos_type fileSize = ifs.tellg();
    if (fileSize < 0)          
        return std::string();  

    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(&bytes[0], fileSize);

    return std::string(&bytes[0], fileSize);
}

void DumpInfo(cgltf_data *data, const char *name) 
{
    printf("CGLTF File Info: %s\n", name);
    printf("------------------------------------------------\n");
    printf("file_size          : %d\n", (int)data->file_size);
    printf("meshes_count       : %d\n", (int)data->meshes_count);
    printf("materials_count    : %d\n", (int)data->materials_count);
    printf("accessors_count    : %d\n", (int)data->accessors_count);
    printf("buffer_views_count : %d\n", (int)data->buffer_views_count);
    printf("buffers_count      : %d\n", (int)data->buffers_count);
    printf("images_count       : %d\n", (int)data->images_count);
    printf("textures_count     : %d\n", (int)data->textures_count);
    printf("samplers_count     : %d\n", (int)data->samplers_count);
    printf("skins_count        : %d\n", (int)data->skins_count);
    printf("cameras_count      : %d\n", (int)data->cameras_count);
    printf("lights_count       : %d\n", (int)data->lights_count);
    printf("nodes_count        : %d\n", (int)data->nodes_count);
    printf("scenes_count       : %d\n", (int)data->scenes_count);
    printf("animations_count   : %d\n", (int)data->animations_count);
    printf("variants_count     : %d\n", (int)data->variants_count);
    printf("json_size          : %d\n", (int)data->json_size);
    printf("bin_size           : %d\n", (int)data->bin_size);
}

// Open a gltf file
static int DumpGLTFInfo(lua_State* L)
{
    char* name = (char*)luaL_checkstring(L, 1);
    if(loaded_files.find(name) != loaded_files.end()) {
        cgltf_data * data = loaded_files[name];
        DumpInfo(data, name);
    }
    else {
        printf("[Error] Issue finding gltf: %s\n", name);
    }
    return 0;
}

// Open a gltf file
static int OpenFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* filename = (char*)luaL_checkstring(L, 1);
    std::string path(filename);
    path.substr(0, path.find_last_of("\\/"));

    cgltf_options options = { cgltf_file_type(0) };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success)
    {
        printf("[Error] Issue parsing file: %s\n", filename);
        lua_pushnil(L);
        return 1;
    }
    // Load in the buffers
    else {
        result = cgltf_load_buffers(&options, data, filename);
        if(result == cgltf_result_success) {
            printf("[Info] Loaded buffers: %s\n", filename);
        } else {
            printf("[Error] Loading buffers: %s  Error: %d\n", filename, (int)result);
            lua_pushnil(L);
            return 1;
        }
    }

    // Check if a file with this name has already been loaded, if so, free and overwrite! (with output)
    if(loaded_files.find(filename) != loaded_files.end()) {
        cgltf_data * old_data = loaded_files[filename];
        cgltf_free(old_data);
        printf("[Warning] File already loaded, replacing: %s\n", filename);
    }
    loaded_files.insert(std::pair<std::string, cgltf_data*>(filename, data));
    lua_pushnumber(L, 1);
    return 1;
}

static void ProcessPrimitive(dmGameObject::HInstance inst, const cgltf_data* data, cgltf_primitive primitive)
{
    int acc_idx = cgltf_accessor_index(data, primitive.indices);
    cgltf_accessor accessor = data->accessors[acc_idx];   
    int bvi = cgltf_buffer_view_index(data, accessor.buffer_view);
    cgltf_buffer_view bv = data->buffer_views[bvi];
    int byte_offset = accessor.offset;
    int bi = cgltf_buffer_index(data, bv.buffer);
    cgltf_buffer buffer = data->buffers[bi];

    // Determine if there are any position elements (vertices)
    if(primitive.type == cgltf_primitive_type_triangles) {
        printf("Found primitive triangles\n");

        for(int i = 0; i<primitive.attributes_count; i++) {
            cgltf_attribute attr = primitive.attributes[i];
            if(attr.type == cgltf_attribute_type_position) {
                printf("position attributes\n");

                cgltf_buffer_view *bv = attr.data->buffer_view;
                const uint8_t* buffer_data = cgltf_buffer_view_data(bv);

                int length = int(bv->size);
                int float_count = length / sizeof(cgltf_float);
                // if(model.counted[pos_attrib] == nil) then
                //     model.stats.vertices = model.stats.vertices + float_count / 3
                //     model.counted[pos_attrib] = true
                // end

                // Get positions (or verts) 
                cgltf_float *verts = new cgltf_float[float_count];
                printf("BufferLen: %d  FloatCount: %d   %s\n", length, float_count, (char *)buffer_data);
                memcpy(verts, buffer_data, length);
                CreatePrimitive(inst, verts, 1, length);
                delete [] verts;
                // local pos_acc = pos_attrib.data
                // local pmin = hmm.HMM_Vec3(pos_acc.min[0], pos_acc.min[1], pos_acc.min[2])
                // local pmax = hmm.HMM_Vec3(pos_acc.max[0], pos_acc.max[1], pos_acc.max[2]) 
                // aabb = calcAABB( aabb, pmin, pmax )
            }
        }
    }
    else if(primitive.type == cgltf_primitive_type_triangle_strip) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive triangle strips\n");
    }
    else if(primitive.type == cgltf_primitive_type_triangle_fan) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive triangle fans\n");
    }
    else if(primitive.type == cgltf_primitive_type_points) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive points\n");
    }
    else if(primitive.type == cgltf_primitive_type_lines) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive liness\n");
    }
    else if(primitive.type == cgltf_primitive_type_line_loop) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive line loops\n");
    }
    else if(primitive.type == cgltf_primitive_type_line_strip) {
        cgltf_attribute pos = primitive.attributes[cgltf_attribute_type_position];
        printf("[Unsupported] Found primitive line strips\n");
    }
}

// Process a node and get the meshes. Each node will become a gameobject.
static void ProcessNode(const cgltf_data* data, const cgltf_node *node) {
    GameCtx* ctx = g_Ctx;
    if(node->name == nullptr) {
        char *new_name = new char[10];
        int node_id = cgltf_node_index(data, node);
        memset(new_name, 0, 10);
        sprintf(new_name, "node_%04d", node_id);
        data->nodes[node_id].name = new_name;
    }

    printf("[Info] Spawning Node: %s\n", node->name);
    if(node->mesh != nullptr) {
        cgltf_mesh* mesh = node->mesh;
        printf("Translation: %d Rotation: %d  Scaling: %d\n", node->has_translation, node->has_rotation, node->has_scale);

        cgltf_float trans[3] = { 0.0f, 0.0f, 0.0f };
        cgltf_float rot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        cgltf_float scale[3] = { 1.0f, 1.0f, 1.0f };
        if(node->has_translation) memcpy(trans, node->translation, sizeof(cgltf_float) * 3);
        if(node->has_rotation) memcpy(rot, node->rotation, sizeof(cgltf_float) * 4);
        if(node->has_scale) memcpy(scale, node->scale, sizeof(cgltf_float) * 3);
        dmGameObject::HInstance inst = SpawnMesh(ctx->m_MeshFactory, trans, rot, scale);

        // Now process the primitives (ie triangles with verts, texels, and colors)
        int prim_count = (int)mesh->primitives_count;
        for(int i = 0; i< prim_count; i++)
        {
            cgltf_primitive primitive = mesh->primitives[i];
            ProcessPrimitive(inst, data, primitive);
        }
    }
}

static void gltf_parse_nodes(cgltf_data *data, cgltf_node *node)
{
	int nodes_count  = (int)node->children_count;
	for(int node_index = 0; node_index < nodes_count; node_index++) {
        cgltf_node * gltf_node = node->children[node_index];
		gltf_node->parent = node;
		gltf_parse_nodes(data, gltf_node);
    }

	if (node->mesh) {
        ProcessNode(data, node);
	}
}

static int ProcessGltf(lua_State *L)
{
    cgltf_data * data = nullptr;
    char* filename = (char*)luaL_checkstring(L, 1);
    if(loaded_files.find(filename) != loaded_files.end()) {
        data = loaded_files[filename];
    }
    else {
        printf("[Error] Issue Processing file: %s\n", filename);
        lua_pushnil(L);
        return 1;
    }

    // Iterate scenes - generate nodes, generate meshes.
    int scenes_count = (int)data->scenes_count;
    printf("Scenes: %d\n", scenes_count);
    for(int i=0; i < scenes_count; i++) 
    {
        cgltf_scene scene = data->scenes[i];
        int nodes_count = (int)scene.nodes_count;
        printf("Scene Nodes: %d %d\n", i, nodes_count);
        for(int j=0; j< nodes_count; j++) {
            printf("Nodes: %d\n", j);
            gltf_parse_nodes(data, scene.nodes[j]);
        }
    }

    lua_pushnumber(L, 1);
    return 1;
}

// Close an opened GLTF file.
static int CloseFile(lua_State *L)
{
    char* filename = (char*)luaL_checkstring(L, 1);
    if(loaded_files.find(filename) != loaded_files.end()) {
        cgltf_data * data = loaded_files[filename];
        cgltf_free(data);
    }
    else {
        printf("[Error] Issue Closing file: %s\n", filename);
    }
    return 0;
}

// Open a gltf file
static int OpenMemory(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* name = (char*)luaL_checkstring(L, 1);
    char* buf = (char*)luaL_checkstring(L, 2);
    int size = luaL_checknumber(L, 3);

    cgltf_options options = { cgltf_file_type(0) };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, buf, size, &data);
    if (result == cgltf_result_success)
    {
        // Check if a file with this name has already been loaded, if so, free and overwrite! (with output)
        if(loaded_files.find(name) != loaded_files.end()) {
            cgltf_data * data = loaded_files[name];
            cgltf_free(data);
            printf("[Warning] File already loaded, replacing: %s\n", name);
        }
        loaded_files.insert(std::pair<std::string, cgltf_data*>(name, data));
        lua_pushnumber(L, 1);
        return 1;
    }
    printf("[Error] Issue loading buffer: %s\n", name);
    lua_pushnil(L);
    return 1;
}

static int Validate(lua_State *L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* name = (char*)luaL_checkstring(L, 1);
    if(loaded_files.find(name) != loaded_files.end()) {
        cgltf_data * data = loaded_files[name];
        cgltf_result result = cgltf_validate(data);
        std::string result_string = (result == cgltf_result_success)? "OK": "FAILED";
        printf("[Validation] Completed on: %s    Result: %s\n", name, result_string.c_str());

        DumpInfo(data, name);
        lua_pushnumber(L, 1);
        return 1;
    } 
    printf("[Error] Issue validating: %s\n", name);
    lua_pushnil(L);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"open_file", OpenFile},
    {"open_memory", OpenMemory},
    {"close", CloseFile},
    {"validate", Validate},
    {"dump_info", DumpGLTFInfo},
    {"process", ProcessGltf},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static void LoadFactory(GameCtx* ctx, dmhash_t path, dmhash_t fragment)
{
    dmLogInfo("LoadFactory: %s", dmHashReverseSafe64(path));
    dmGameObject::HInstance go = dmGameObject::GetInstanceFromIdentifier(ctx->m_MainCollection, path);
    if (go == 0)
    {
        dmLogError("Main collection does not have a game object named %s", dmHashReverseSafe64(path));
        ctx->m_HasError = true;
        return;
    }

    uint32_t factory_type_index = dmGameObject::GetComponentTypeIndex(ctx->m_MainCollection, dmHashString64("factoryc"));
    uint32_t component_type_index;
    dmGameObject::Result r = dmGameObject::GetComponent(go, fragment, &component_type_index, (dmGameObject::HComponent*)&ctx->m_MeshFactory, (dmGameObject::HComponentWorld*)&ctx->m_FactoryWorld);
    assert(dmGameObject::RESULT_OK == r);
    assert(factory_type_index == component_type_index);
}


static void UnloadFactory(GameCtx* ctx, dmhash_t path, dmhash_t fragment)
{
    dmLogInfo("UnloadFactory: %s", dmHashReverseSafe64(path));
}

static void InitGame(GameCtx* ctx)
{
    dmLogInfo("InitGame");

    const char* main_collection_path = dmConfigFile::GetString(ctx->m_ConfigFile, "bootstrap.main_collection", 0);

    // Get the main collection
    // Needs to be called after the resource types have been registered
    dmResource::Result res = dmResource::Get(ctx->m_Factory, main_collection_path, (void**) &ctx->m_MainCollection);
    if (dmResource::RESULT_OK != res)
    {
        dmLogError("Failed to get main collection '%s'", main_collection_path);
        ctx->m_IsInitialized = true;
        ctx->m_HasError = true;
        return;
    }
    assert(ctx->m_MainCollection != 0);

    ctx->m_LastFrameTime = 0;
    ctx->m_SpawnTimerInterval = 1;
    ctx->m_Meshes.SetCapacity(1000); // TODO: This should be from mesh config really

    LoadFactory(ctx, dmHashString64("/meshes"), dmHashString64("meshfactory"));
    ctx->m_IsInitialized = true;
}

static void ExitGame(GameCtx* ctx)
{
    dmLogInfo("ExitGame");
    if(ctx == nullptr) return;

    if (ctx->m_MainCollection)
    {
        dmResource::Release(ctx->m_Factory, ctx->m_MainCollection);
        ctx->m_MainCollection = 0;
    }
}

static dmExtension::Result AppInitializecgltf_lib(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializecgltf_lib");

    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initializecgltf_lib(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    GameCtx* ctx = new GameCtx();
    g_Ctx = ctx; // TODO: Figure out how to store it as the extension context

    ctx->m_Factory = params->m_ResourceFactory;
    ctx->m_ConfigFile = params->m_ConfigFile;
    // copy any other contexts needed

    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizecgltf_lib(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizecgltf_lib");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalizecgltf_lib(dmExtension::Params* params)
{
    dmLogInfo("Finalizecgltf_lib");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdatecgltf_lib(dmExtension::Params* params)
{
    dmLogInfo("OnUpdatecgltf_lib");
    // GameCtx* ctx = g_Ctx;
    // uint64_t last_time = ctx->m_LastFrameTime;
    // ctx->m_LastFrameTime = dmTime::GetTime();
    // float dt = (ctx->m_LastFrameTime - last_time) / 1000000.0f;    
    return dmExtension::RESULT_OK;
}

static void OnEventcgltf_lib(dmExtension::Params* params, const dmExtension::Event* event)
{
    switch(event->m_Event)
    {
        case dmExtension::EVENT_ID_ACTIVATEAPP:
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_ACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_DEACTIVATEAPP:
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_DEACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_ICONIFYAPP:
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_ICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_DEICONIFYAPP:
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_DEICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_ENGINE_INITIALIZED:
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_ENGINE_INITIALIZED");
            InitGame(g_Ctx);
            break;
        case dmExtension::EVENT_ID_ENGINE_DELETE:
            printf("OnEventcgltf_lib - EVENT_ID_ENGINE_DELETE\n");
            dmLogInfo("OnEventcgltf_lib - EVENT_ID_ENGINE_DELETE");
            ExitGame(g_Ctx);
            break;
        default:
            dmLogWarning("OnEventcgltf_lib - Unknown event id");
            break;
    }
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// cgltf_lib is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(cgltf_lib, LIB_NAME, AppInitializecgltf_lib, AppFinalizecgltf_lib, Initializecgltf_lib, OnUpdatecgltf_lib, OnEventcgltf_lib, Finalizecgltf_lib)
