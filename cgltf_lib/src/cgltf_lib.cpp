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
    char * name = (char *)luaL_checkstring(L, 1);
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 2);
    if(data) {
        DumpInfo(data, name);
    }
    else {
        printf("[Error] Issue finding gltf: %s\n", name);
    }
    return 0;
}

static void make_name( char *group, char * name, int id )
{
    name = new char[64];
    memset(name, 0, 64);
    sprintf(name, "%s%03d", group, id );
}

// Open a gltf file
static int lib_cgltf_parse_file(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* filename = (char*)luaL_checkstring(L, 1);
    std::string path(filename);

    cgltf_options options = { cgltf_file_type(0) };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success)
    {
        printf("[Error] Issue parsing file: %s\n", filename);
        lua_pushnil(L);
        return 1;
    }
    lua_pushlightuserdata(L, (void *)data);
    return 1;
}

static int lib_cgltf_load_buffers(lua_State *L)
{
    char* filepath = (char*)luaL_checkstring(L, 1);
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 2);
    
    // Load in the buffers
    cgltf_options options = { cgltf_file_type(0) };    
    cgltf_result result = cgltf_load_buffers(&options, data, filepath);
    if(result == cgltf_result_success) {
        printf("[Info] Loaded buffers: %s\n", filepath);
    } else {
        printf("[Error] Loading buffers: %s  Error: %d\n", filepath, (int)result);
        lua_pushnil(L);
        return 1;
    }

    lua_pushlightuserdata(L, (void *)data);
    return 1;
}


static int lib_cgltf_validate(lua_State *L)
{
    DM_LUA_STACK_CHECK(L, 1);

    char* name = (char*)luaL_checkstring(L, 1);
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 2);
    if(data) {
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

static int lib_cgltf_buffer_view_data(lua_State *L)
{
    cgltf_buffer_view * bv = (cgltf_buffer_view *)lua_touserdata(L, 1);
    if(bv) {
        uint8_t *data = (uint8_t *)cgltf_buffer_view_data(bv);
        lua_pushlightuserdata(L, data);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int lib_cgltf_accessor_read_float(lua_State *L)
{
    cgltf_accessor * acc = (cgltf_accessor *)lua_touserdata(L, 1);
    int index           = lua_tonumber(L, 2);
    int count           = lua_tonumber(L, 3);
    float *data = new float[count];
    cgltf_bool result = cgltf_accessor_read_float(acc, index, data, count);
    if(result == 1) {
        lua_newtable(L);
        for(int i=0; i<count; i++) {
            lua_pushinteger(L, i+1);
            lua_pushnumber(L, data[i]);
            lua_settable(L, -3);
        }
        delete [] data;
        return 1;
    }
    delete [] data;
    lua_pushnil(L);
    return 1;
}

static int lib_get_images_count(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    lua_pushnumber(L, data->images_count);
    return 1;
}

static int lib_get_image(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    int i = lua_tonumber(L, 2);
    cgltf_image * image = &data->images[i];
    lua_pushlightuserdata(L, image);
    return 1;
}

static int lib_get_image_name(lua_State *L) {
    cgltf_image * img = (cgltf_image *)lua_touserdata(L, 1);

    char *name = img->name;
    lua_pushstring(L, name);
    return 1;
}

static int lib_get_image_uri(lua_State *L) {
    cgltf_image * img = (cgltf_image *)lua_touserdata(L, 1);
    lua_pushstring(L, img->uri);
    return 1;
}

static int lib_get_image_bufferview(lua_State *L) {
    cgltf_image * img = (cgltf_image *)lua_touserdata(L, 1);
    lua_pushlightuserdata(L, img->buffer_view);
    return 1;
}

static int lib_get_buffer_view(lua_State *L) {
    cgltf_buffer_view * bv = (cgltf_buffer_view *)lua_touserdata(L, 1);
    if(bv == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);

    lua_pushstring(L, "size");
    lua_pushnumber(L, bv->size);
    lua_settable(L, -3);

    lua_pushstring(L, "stride");
    lua_pushnumber(L, bv->stride);
    lua_settable(L, -3);

    lua_pushstring(L, "offset");
    lua_pushnumber(L, bv->offset);
    lua_settable(L, -3);

    lua_pushstring(L, "type");
    lua_pushnumber(L, bv->type);
    lua_settable(L, -3);

    return 1;
}


static int lib_get_buffer_view_size(lua_State *L) {
    cgltf_buffer_view * bv = (cgltf_buffer_view *)lua_touserdata(L, 1);
    lua_pushnumber(L, bv->size);
    return 1;
}

static int lib_get_buffer_view_index_data(lua_State *L) {
    cgltf_buffer_view * bv = (cgltf_buffer_view *)lua_touserdata(L, 1);
    int datasize = lua_tonumber(L, 2);
    // printf("BV Size: %d  BV Offset %d  BV Stride %d\n", (int)bv->size, (int)bv->offset, (int)bv->stride);
    if(bv) {
        uint8_t *data = (uint8_t *)cgltf_buffer_view_data(bv);
        int count = bv->size;
        lua_newtable(L);
        if(datasize == 1) {
            uint8_t *dataptr = data;
            for(int i=0; i<count; i++ ) {
                lua_pushinteger(L, i+1);
                lua_pushinteger(L, dataptr[i]+1);
                lua_settable(L, -3);
            }
        }
        else if(datasize == 2)
        {
            count = bv->size/2;  // Default is uint16
            uint16_t *dataptr = (uint16_t*)data;
            for(int i=0; i<count; i++ ) {
                lua_pushinteger(L, i+1);
                lua_pushinteger(L, (int)dataptr[i]+1);
                lua_settable(L, -3);
            }
        }
        else if(datasize == 4)
        {
            count = bv->size/4; 
            uint32_t *dataptr = (uint32_t*)data;
            for(int i=0; i<count; i++)
            {
                lua_pushinteger(L, i+1);
                lua_pushinteger(L, dataptr[i]+1);
                lua_settable(L, -3);
            }
        }
        else {
            printf("[Error] Primitive buffer invalid data size: %d\n", datasize);
            lua_pushnil(L);
        }
    }
    else {
        printf("[Error] Primitive buffer invalid buffer_view.\n");
        lua_pushnil(L);
    }
    return 1;
}

static int lib_get_buffer_view_vertex_data(lua_State *L) {
    cgltf_buffer_view * bv = (cgltf_buffer_view *)lua_touserdata(L, 1);
    if(bv) {
        uint8_t *data = (uint8_t *)cgltf_buffer_view_data(bv);
        int count = bv->size/4;
        printf("Stride: %d\n", (int)bv->stride);
        lua_newtable(L);
        float *dataptr = (float *)data;
        for(int i=0; i<count; i++ ) {
            lua_pushinteger(L, i+1);
            lua_pushnumber(L, *dataptr++);
            lua_settable(L, -3);
        }
    }
    else {
        printf("[Error] Primitive buffer invalid buffer_view.\n");
        lua_pushnil(L);
    }
    return 1;
}

static void addAccessor(lua_State *L, cgltf_data * data, cgltf_accessor *acc)
{
    if(acc == nullptr) return; 

    lua_pushstring(L, "addr" );
    lua_pushlightuserdata(L, acc);
    lua_settable(L, -3);

    char *name = acc->name;
    if(name == nullptr) {
        int acc_id = cgltf_accessor_index(data, acc);
        make_name(name, "acc_", acc_id);
    }

    lua_pushstring(L, "name" );
    lua_pushstring(L, name);
    lua_settable(L, -3);

    lua_pushstring(L, "component_type" );
    lua_pushinteger(L, (int)acc->component_type);
    lua_settable(L, -3);

    lua_pushstring(L, "normalized" );
    lua_pushboolean(L, acc->normalized);
    lua_settable(L, -3);    

    lua_pushstring(L, "type" );
    lua_pushinteger(L, (int)acc->type);
    lua_settable(L, -3);

    lua_pushstring(L, "count" );
    lua_pushinteger(L, (int)acc->count);
    lua_settable(L, -3);

    lua_pushstring(L, "offset" );
    lua_pushinteger(L, (int)acc->offset);
    lua_settable(L, -3);

    lua_pushstring(L, "stride" );
    lua_pushinteger(L, (int)acc->stride);
    lua_settable(L, -3);

    lua_pushstring(L, "buffer_view" );
    lua_pushlightuserdata(L, acc->buffer_view);
    lua_settable(L, -3);    

    lua_pushstring(L, "has_min" );
    lua_pushboolean(L, acc->has_min);
    lua_settable(L, -3);    

    lua_pushstring(L, "has_max" );
    lua_pushboolean(L, acc->has_max);
    lua_settable(L, -3);    

    lua_pushstring(L, "min");
    lua_newtable(L);    
    for(int i=0; i<16; i++) {
        lua_pushinteger(L, i+1);
        lua_pushnumber(L, acc->min[i]);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);   

    lua_pushstring(L, "max");
    lua_newtable(L);    
    for(int i=0; i<16; i++) {
        lua_pushinteger(L, i+1);
        lua_pushnumber(L, acc->max[i]);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);   
}

static int lib_get_textures_count(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    lua_pushnumber(L, data->textures_count);
    return 1;
}

static int lib_get_texture(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    int i = lua_tonumber(L, 2);
    cgltf_texture * tex = &data->textures[i];
    lua_pushlightuserdata(L, tex);
    return 1;
}

static int lib_get_texture_name(lua_State *L) {
    cgltf_texture * tex = (cgltf_texture *)lua_touserdata(L, 1);
    char *name = tex->name;
    lua_pushstring(L, name);
    return 1;
}

static int lib_get_texture_image(lua_State *L) {
    cgltf_texture * tex = (cgltf_texture *)lua_touserdata(L, 1);
    lua_pushlightuserdata(L, tex->image);
    return 1;
}

static int lib_get_materials_count(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    lua_pushnumber(L, data->materials_count);
    return 1;
}

static void fetchMaterial(lua_State *L, cgltf_data *data, cgltf_material *mat)
{
    lua_pushstring(L, "addr" );
    lua_pushlightuserdata(L, mat);
    lua_settable(L, -3);

    char *name = mat->name;
    if(name == nullptr) {
        int mat_id = cgltf_material_index(data, mat);
        make_name(name, "mat_", mat_id);
    }

    lua_pushstring(L, "name" );
    lua_pushstring(L, name);
    lua_settable(L, -3);

    lua_pushstring(L, "alpha_mode" );
    lua_pushinteger(L, (int)mat->alpha_mode);
    lua_settable(L, -3);

    lua_pushstring(L, "alpha_cutoff" );
    lua_pushnumber(L, mat->alpha_cutoff);
    lua_settable(L, -3);

    lua_pushstring(L, "double_sided" );
    lua_pushboolean(L, mat->double_sided);
    lua_settable(L, -3);

    lua_pushstring(L, "has_pbr_metallic_roughness" );
    lua_pushboolean(L, mat->has_pbr_metallic_roughness);
    lua_settable(L, -3);

    lua_pushstring(L, "has_pbr_specular_glossiness" );
    lua_pushboolean(L, mat->has_pbr_specular_glossiness);
    lua_settable(L, -3);

    lua_pushstring(L, "has_specular" );
    lua_pushboolean(L, mat->has_specular);
    lua_settable(L, -3);

    lua_pushstring(L, "has_emissive_strength" );
    lua_pushboolean(L, mat->has_emissive_strength);
    lua_settable(L, -3);

    lua_pushstring(L, "base_color_texture" );
    lua_pushlightuserdata(L, mat->pbr_metallic_roughness.base_color_texture.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "metallic_roughness_texture" );
    lua_pushlightuserdata(L, mat->pbr_metallic_roughness.metallic_roughness_texture.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "normal_texture" );
    lua_pushlightuserdata(L, mat->normal_texture.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "occlusion_texture" );
    lua_pushlightuserdata(L, mat->occlusion_texture.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "emissive_texture" );
    lua_pushlightuserdata(L, mat->emissive_texture.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "base_color_factor");
    lua_newtable(L);

        lua_pushinteger(L, 0);
        lua_pushnumber(L, mat->pbr_metallic_roughness.base_color_factor[0]);
        lua_settable(L, -3);
        lua_pushinteger(L, 1);
        lua_pushnumber(L, mat->pbr_metallic_roughness.base_color_factor[1]);
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushnumber(L, mat->pbr_metallic_roughness.base_color_factor[2]);
        lua_settable(L, -3);
        lua_pushinteger(L, 3);
        lua_pushnumber(L, mat->pbr_metallic_roughness.base_color_factor[3]);
        lua_settable(L, -3);

    lua_settable(L, -3);

    lua_pushstring(L, "metallic_factor");  
    lua_pushnumber(L, mat->pbr_metallic_roughness.metallic_factor); 
    lua_settable(L, -3);

    lua_pushstring(L, "roughness_factor");  
    lua_pushnumber(L, mat->pbr_metallic_roughness.roughness_factor); 
    lua_settable(L, -3);

    lua_pushstring(L, "emissive_factor");
    lua_newtable(L);

        lua_pushinteger(L, 0);
        lua_pushnumber(L, mat->emissive_factor[0]);
        lua_settable(L, -3);
        lua_pushinteger(L, 1);
        lua_pushnumber(L, mat->emissive_factor[1]);
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushnumber(L, mat->emissive_factor[2]);
        lua_settable(L, -3);

    lua_settable(L, -3);    
}

static int lib_get_material(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    cgltf_material * mat = (cgltf_material *)lua_touserdata(L, 2);
    lua_newtable(L);
    fetchMaterial(L, data, mat);
    return 1;
}

static int lib_get_material_index(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    int i = lua_tonumber(L, 2);
    lua_newtable(L);
    cgltf_material * mat = &data->materials[i];

    fetchMaterial(L, data, mat);
    return 1;
}

static int lib_get_meshes_count(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    lua_pushnumber(L, data->meshes_count);
    return 1;
}

static void addMesh(lua_State *L, cgltf_data *data, cgltf_mesh *mesh)
{
    lua_pushstring(L, "addr" );
    lua_pushlightuserdata(L, mesh);
    lua_settable(L, -3);

    char *name = mesh->name;
    if(name == nullptr) {
        int mesh_id = cgltf_mesh_index(data, mesh);
        make_name(name, "mesh_", mesh_id);
    }

    lua_pushstring(L, "name" );
    lua_pushstring(L, name);
    lua_settable(L, -3);

    lua_pushstring(L, "primitives_count" );
    lua_pushinteger(L, mesh->primitives_count);
    lua_settable(L, -3);

    lua_pushstring(L, "primitives" );
    lua_pushlightuserdata(L, mesh->primitives);
    lua_settable(L, -3);
}

static int lib_get_mesh_index(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    int i = lua_tonumber(L, 2);   
    cgltf_mesh * mesh = &data->meshes[i];
    lua_newtable(L);
    addMesh(L, data, mesh);
    return 1;
}

static int lib_get_mesh(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    cgltf_mesh * mesh = (cgltf_mesh *)lua_touserdata(L, 2);
    lua_newtable(L);
    addMesh(L, data, mesh);
    return 1;
}

static int lib_get_mesh_primitive(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    cgltf_mesh * mesh = (cgltf_mesh *)lua_touserdata(L, 2);
    int i = lua_tonumber(L, 2);

    lua_newtable(L);
    cgltf_primitive * prim = &mesh->primitives[i];

    lua_pushstring(L, "addr" );
    lua_pushlightuserdata(L, prim);
    lua_settable(L, -3);

    lua_pushstring(L, "indices" );
    lua_pushlightuserdata(L, prim->indices);
    lua_settable(L, -3);

    lua_pushstring(L, "material" );
    lua_pushlightuserdata(L, prim->material);
    lua_settable(L, -3);    

    lua_pushstring(L, "type" );
    lua_pushinteger(L, prim->type);
    lua_settable(L, -3);

    lua_pushstring(L, "attributes_count");
    lua_pushinteger(L, prim->attributes_count);
    lua_settable(L, -3);

    lua_pushstring(L, "attributes");
    lua_newtable(L);
    for(int i=0; i<prim->attributes_count; i++) {
        cgltf_attribute attrib = prim->attributes[i];
        lua_pushinteger(L, i+1);
        lua_newtable(L);

            lua_pushstring(L, "name" );
            lua_pushstring(L, attrib.name);
            lua_settable(L, -3);
            lua_pushstring(L, "type" );
            lua_pushinteger(L, (int)attrib.type);
            lua_settable(L, -3);
            lua_pushstring(L, "index" );
            lua_pushinteger(L, attrib.index);
            lua_settable(L, -3);

            lua_pushstring(L, "data" );
            lua_newtable(L);
            addAccessor(L, data, (cgltf_accessor *)attrib.data);
            lua_settable(L, -3);
        
        lua_settable(L, -3);
    }

    lua_settable(L, -3);

    return 1;    
}

static int lib_get_scene_nodes_count(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    lua_pushnumber(L, data->scene->nodes_count);
    return 1;
}

static void addNodeData(lua_State *L, cgltf_data *data, cgltf_node *node)
{
    
    lua_pushstring(L, "addr" );
    lua_pushlightuserdata(L, node);
    lua_settable(L, -3);

    char *name = node->name;
    if(name == nullptr) {
        int node_id = cgltf_node_index(data, node);
        make_name(name, "node_", node_id);
    }

    lua_pushstring(L, "name" );
    lua_pushstring(L, name);
    lua_settable(L, -3);

    lua_pushstring(L, "has_translation" );
    lua_pushboolean(L, node->has_translation);
    lua_settable(L, -3);

    lua_pushstring(L, "has_rotation" );
    lua_pushboolean(L, node->has_rotation);
    lua_settable(L, -3);

    lua_pushstring(L, "has_scale" );
    lua_pushboolean(L, node->has_scale);
    lua_settable(L, -3);

    lua_pushstring(L, "has_matrix" );
    lua_pushboolean(L, node->has_matrix);
    lua_settable(L, -3);

    lua_pushstring(L, "translation");
    lua_newtable(L);

        lua_pushinteger(L, 0);
        lua_pushnumber(L, node->translation[0]);
        lua_settable(L, -3);
        lua_pushinteger(L, 1);
        lua_pushnumber(L, node->translation[1]);
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushnumber(L, node->translation[2]);
        lua_settable(L, -3);

    lua_settable(L, -3);   

    lua_pushstring(L, "rotation");
    lua_newtable(L);

        lua_pushinteger(L, 0);
        lua_pushnumber(L, node->rotation[0]);
        lua_settable(L, -3);
        lua_pushinteger(L, 1);
        lua_pushnumber(L, node->rotation[1]);
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushnumber(L, node->rotation[2]);
        lua_settable(L, -3);
        lua_pushinteger(L, 3);
        lua_pushnumber(L, node->rotation[3]);
        lua_settable(L, -3);

    lua_settable(L, -3);   

    lua_pushstring(L, "scale");
    lua_newtable(L);

        lua_pushinteger(L, 0);
        lua_pushnumber(L, node->scale[0]);
        lua_settable(L, -3);
        lua_pushinteger(L, 1);
        lua_pushnumber(L, node->scale[1]);
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushnumber(L, node->scale[2]);
        lua_settable(L, -3);

    lua_settable(L, -3);   

    lua_pushstring(L, "matrix");
    lua_newtable(L);    
    for(int i=0; i<16; i++) {
        lua_pushinteger(L, i+1);
        lua_pushnumber(L, node->matrix[i]);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);   

    lua_pushstring(L, "mesh" );
    lua_pushlightuserdata(L, node->mesh);
    lua_settable(L, -3);

    lua_pushstring(L, "skin" );
    lua_pushlightuserdata(L, node->skin);
    lua_settable(L, -3);

    lua_pushstring(L, "camera" );
    lua_pushlightuserdata(L, node->camera);
    lua_settable(L, -3);    

    lua_pushstring(L, "light" );
    lua_pushlightuserdata(L, node->light);
    lua_settable(L, -3);   

    lua_pushstring(L, "children_count" );
    lua_pushinteger(L, node->children_count);
    lua_settable(L, -3);
}


static int lib_get_scene_node(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    int i = lua_tonumber(L, 2);
    lua_newtable(L);
    cgltf_node * node = data->scene->nodes[i];
    addNodeData(L, data, node);
    return 1;
}

static int lib_get_node_child(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    cgltf_node * pnode = (cgltf_node *)lua_touserdata(L, 2);
    int i = lua_tonumber(L, 3);
    lua_newtable(L);
    cgltf_node * node = pnode->children[i];

    addNodeData(L, data, node);
    return 1;
}

static int lib_get_accessor(lua_State *L) {
    cgltf_data * data = (cgltf_data *)lua_touserdata(L, 1);
    cgltf_accessor * acc = (cgltf_accessor *)lua_touserdata(L, 2);
    lua_newtable(L);
    addAccessor(L, data, acc);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"cgltf_parse_file", lib_cgltf_parse_file},
    {"cgltf_load_buffers", lib_cgltf_load_buffers},
    {"cgltf_validate", lib_cgltf_validate},
    {"cgltf_buffer_view_data", lib_cgltf_buffer_view_data},
    {"cgltf_accessor_read_float", lib_cgltf_accessor_read_float},
    
    {"get_images_count", lib_get_images_count},
    {"get_image", lib_get_image},
    {"get_image_name", lib_get_image_name},
    {"get_image_uri", lib_get_image_uri},
    {"get_image_buffer_view", lib_get_image_bufferview},

    {"get_buffer_view", lib_get_buffer_view},
    {"get_buffer_view_size", lib_get_buffer_view_size},
    {"get_buffer_view_index_data", lib_get_buffer_view_index_data},
    {"get_buffer_view_vertex_data", lib_get_buffer_view_vertex_data},

    {"get_textures_count", lib_get_textures_count},
    {"get_texture", lib_get_texture},
    {"get_texture_name", lib_get_texture_name},
    {"get_texture_image", lib_get_texture_image},

    {"get_materials_count", lib_get_materials_count},
    {"get_material_index", lib_get_material_index},
    {"get_material", lib_get_material},

    {"get_meshes_count", lib_get_meshes_count},
    {"get_mesh_index", lib_get_mesh_index},
    {"get_mesh", lib_get_mesh},

    {"get_mesh_primitive", lib_get_mesh_primitive},

    {"get_scene_nodes_count", lib_get_scene_nodes_count},
    {"get_scene_node", lib_get_scene_node},
    {"get_node_child", lib_get_node_child},

    {"get_accessor", lib_get_accessor},

    {"dump_info", DumpGLTFInfo},
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
