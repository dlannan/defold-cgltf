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

    dmGameSystem::HCollectionProxyWorld     m_LevelCollectionProxyWorld;
    dmGameSystem::HCollectionProxyComponent m_LevelCollectionProxy;

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
    bool    m_CollectionLoading;
    bool    m_LevelInitialized;

    GameCtx()
    {
        memset(this, 0, sizeof(*this));
    }
};

static GameCtx* g_Ctx = 0;

static dmGameObject::HInstance SpawnMesh(dmGameSystem::HFactoryComponent factory, cgltf_float* _pos, cgltf_float * _rot, cgltf_float * _scl)
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

    dmhash_t meshid = dmGameObject::CreateInstanceId();
    dmGameObject::HPropertyContainer properties = 0;

    dmGameObject::HInstance instance = 0;
    dmGameObject::Result result = dmGameSystem::CompFactorySpawn(ctx->m_FactoryWorld, factory, ctx->m_LevelCollection,
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

    if (result == cgltf_result_success) {
        cgltf_load_buffers(&options, data, path.c_str());
        printf("[Info] Loaded buffers: %s\n", filename);
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

// Process a node and get the meshes. Each node will become a gameobject.
static void ProcessNode(cgltf_scene scene, const cgltf_node *node) {
    GameCtx* ctx = g_Ctx;
    printf("[Info] Spawning Mesh: %s\n", node->name);
    if(node->mesh != nullptr) {
        //dmGameObject::HInstance inst = SpawnMesh(ctx->m_MeshFactory, node->translation, node->rotation, node->scale);
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
    uint32_t scenes_count = (uint32_t)data->scenes_count;
    printf("Scenes: %d\n", scenes_count);
    for(uint32_t i=0; i < 1; i++) 
    {
        cgltf_scene scene = data->scenes[i];
        uint32_t nodes_count = (uint32_t)scene.nodes_count;
        printf("Scene Nodes: %d %d\n", i, nodes_count);
        for(uint32_t j=0; j< 1; j++) {
            printf("Nodes: %d\n", j);
            ProcessNode(scene, scene.nodes[j]);
        }
    }
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

static void LoadCollection(GameCtx* ctx, dmhash_t path, dmhash_t fragment)
{
    dmLogInfo("LoadCollection: %s", dmHashReverseSafe64(path));

    assert(ctx->m_CollectionLoading == false);
    assert(ctx->m_LevelCollection == 0);

    //      Compat: Supported (private gameobject.h)
    uint32_t proxy_type_index = dmGameObject::GetComponentTypeIndex(ctx->m_MainCollection, dmHashString64("collectionproxyc"));
    dmhash_t go_name = dmHashString64("/levels");
    dmGameObject::HInstance go = dmGameObject::GetInstanceFromIdentifier(ctx->m_MainCollection, go_name);
    if (go == 0)
    {
        dmLogError("Main collection does not have a game object named %s", dmHashReverseSafe64(go_name));
        ctx->m_HasError = true;
        return;
    }

    uint32_t factory_type_index = dmGameObject::GetComponentTypeIndex(ctx->m_MainCollection, dmHashString64("factoryc"));
    uint32_t component_type_index;
    dmGameObject::Result r = dmGameObject::GetComponent(go, dmHashString64("meshfactory"), &component_type_index, (dmGameObject::HComponent*)&ctx->m_MeshFactory, (dmGameObject::HComponentWorld*)&ctx->m_FactoryWorld);
    assert(dmGameObject::RESULT_OK == r);
    assert(factory_type_index == component_type_index);

    ctx->m_CollectionLoading = false;
}


static void UnloadCollection(GameCtx* ctx, dmhash_t path, dmhash_t fragment)
{
    dmLogInfo("UnloadCollection: %s", dmHashReverseSafe64(path));

    assert(ctx->m_CollectionLoading == false);
    assert(ctx->m_LevelCollection != 0);
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

    LoadCollection(ctx, dmHashString64("/levels"), dmHashString64("level1"));
    ctx->m_IsInitialized = true;
}

static void ExitGame(GameCtx* ctx)
{
    dmLogInfo("ExitGame");
    if(ctx == nullptr) return;

    if (ctx->m_LevelCollection)
    {
        dmResource::Release(ctx->m_Factory, ctx->m_LevelCollection);
        ctx->m_LevelCollection = 0;
    }

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
