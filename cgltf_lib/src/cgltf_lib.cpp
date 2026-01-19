// myextension.cpp
// Extension lib defines
#define LIB_NAME "cgltf_lib"
#define MODULE_NAME "cgltf"

// include the Defold SDK
#include <dmsdk/sdk.h>

#define CGLTF_EXPORT extern

/* Expose api for use by external */
#include "cgltf/cgltf.h"
#include "cgltf/cgltf_write.h"

/* cgltf files to make a simple cgltf lib */
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf/cgltf_write.h"

#include <vector>
#include <map>
#include <string>

static std::map<std::string, cgltf_data*>     loaded_files;

// Open a gltf file
static int OpenFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* filename = (char*)luaL_checkstring(L, 1);

    cgltf_options options = { cgltf_file_type(0) };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result == cgltf_result_success)
    {
        // Check if a file with this name has already been loaded, if so, free and overwrite! (with output)
        if(loaded_files.find(filename) != loaded_files.end()) {
            cgltf_data * old_data = loaded_files[filename];
            cgltf_free(old_data);
            printf("[Warning] File already loaded, replacing: %s", filename);
        }
        loaded_files.insert(std::pair<std::string, cgltf_data*>(filename, data));
        lua_pushnumber(L, 1);
        return 1;
    }    
    printf("[Error] Issue loading file: %s", filename);
    lua_pushnil(L);
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
        printf("[Error] Issue loading file: %s", filename);
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
            cgltf_data * old_data = loaded_files[name];
            cgltf_free(old_data);
            printf("[Warning] File already loaded, replacing: %s", name);
        }
        loaded_files.insert(std::pair<std::string, cgltf_data*>(name, data));
        lua_pushnumber(L, 1);
        return 1;
    }
    printf("[Error] Issue loading file: %s", name);
    lua_pushnil(L);
    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"open_file", OpenFile},
    {"open_memory", OpenMemory},
    {"close", CloseFile},
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

static dmExtension::Result AppInitializecgltf_lib(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializecgltf_lib");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initializecgltf_lib(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
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
