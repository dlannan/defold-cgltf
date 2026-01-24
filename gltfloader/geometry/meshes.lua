
-- A mesh is a specific mesh resource. 
--   It contains _only_
--   - vertices         * required
--   - indices          * required
--   - texcoords 
--   - normals
--   - colors
--   - tangents 
--   - custom stream data. 

-- These are created as buffers and can be used as needed. 

local utils         = require("gltfloader.utils")
local imageutils 	= require("gltfloader.image-utils")

local tinsert       = table.insert

-- --------------------------------------------------------------------------------------

-- local shc           = require("tools.shader_compiler.shc_compile").init( "dim", false )

local bufferid        = 1

-- ----------------------------------------------------------------------------------------

local all_meshes = {
    materials   = {},           -- A material shader cache.
}

local GLTF_VERTEXFORMAT = {
    FLOAT2         = 2, 
    FLOAT3         = 3,
    FLOAT4         = 4,
}

-- ----------------------------------------------------------------------------------------

local MESH_TYPE    = {
    MTYPE_TRIANGLES   = 1, 
    MTYPE_TRI_STRIP   = 2,
    MTYPE_POINTS      = 3,        -- These use special shaders
    MTYPE_LINES       = 4,        -- These use special shaders
}

-- ----------------------------------------------------------------------------------------

local mesh = {
    id          = 0,        -- internal id for the engine
    priority    = 0,        -- rendering information
    binid       = 0,        -- current render bin assignment (transparent/opaque/custom)
    type        = MESH_TYPE.MTYPE_TRIANGLES,
    vertices    = nil,      -- vert buf 
    indices     = nil,      -- This can be nil. If so, triangles are rendered in order.
    streams     = {},       -- The above attached streams of data (uvs, norms, etc)
}

--- Load the types into the main library (so external systems can use it)
for k,v in pairs(MESH_TYPE) do 
    mesh[k]=v
end

-- ----------------------------------------------------------------------------------------
-- Buffer creation tool
mesh.create_buffer     = function(name, buffers, pid)

    -- Make a mesh table with the info we need to build the pipeline and bindings.
    -- Buffers need to be interleaved with data sets, so we do this here. 
    local vertcount     = 0 

    local stride      = 0
    local attribs     = {}
    local float_size  = 4
    local attribsize  = 3

    if(buffers.vertices) then 
        
        vertcount = utils.tcount(buffers.vertices) / 3
        tinsert(attribs, { 
            name = "position", 
            count = 3, 
            type = buffer.VALUE_TYPE_FLOAT32, 
            data = buffers.vertices 
        })

        if(buffers.uvs) then 
            attribsize = attribsize + 3
            tinsert(attribs, { 
                name = "texcoord", 
                count = 2, 
                type = buffer.VALUE_TYPE_FLOAT32, 
                data = buffers.uvs 
            })
        end
        if(buffers.normals) then 
            attribsize = attribsize + 3
            tinsert(attribs, { 
                name = "normal", 
                count = 3, 
                type = buffer.VALUE_TYPE_FLOAT32, 
                data = buffers.normals 
            })
        end
        if(buffers.colors) then 
            attribsize = attribsize + 4
            tinsert(attribs, { 
                name = "color", 
                count = 4, 
                type = buffer.VALUE_TYPE_FLOAT32, 
                data = buffers.colors 
            })
        end
    end

    local buffs = {
        vbuf   = nil,
        vcount  = vertcount,
        ibuf   = nil,
        icount  = 0,
        sbuf   = nil,
        scount  = 0,
        stride  = stride,
        attrs   = attribs,
        depth   = {},
        pid     = pid,
    }

    if(buffers.indices) then 
        buffs.icount = buffers.icount
        buffs.index_type = buffers.itype
    end

    if(buffers.vertices) then 

        local all_attribs = {}
        for i,v in ipairs(attribs) do
            tinsert(all_attribs,        
            {
                name  = hash(v.name),
                type  = v.type,
                count = v.count
            })
        end

        local datasize = buffers.icount
        if(datasize == 0) then 
            datasize = vertcount *  3
        end
        local buffer_handle = buffer.create(datasize, all_attribs)

        for i, v in ipairs(attribs) do
            local stream = buffer.get_stream(buffer_handle, hash(v.name))
            -- transfer vertex data to buffer
            if(buffers.indices) then 
                pprint("INDICIES: ", datasize)
                for bi, index in ipairs(buffers.indices) do
                    stream[bi] = tonumber(v.data[index])
                end        
            else 
                pprint("VERTICES: ", datasize)
                for bi = 0, vertcount -1 do
                    stream[bi+1] = tonumber(v.data[bi])
                end        
            end
        end
        local buffer_name = string.format("/mesh_buffer_%s_%03d.bufferc", name, pid or 1)

        local success, result = pcall(resource.get_buffer, buffer_name)
        local my_buffer = hash(buffer_name)
        if not success then
            my_buffer = resource.create_buffer(buffer_name, { buffer = buffer_handle })
        end        
        
        -- buffer should now have interlaced data
        local buffer_desc           = {}
        buffer_desc.type         = "VERTEXBUFFER"
        buffer_desc.buffer       = my_buffer
        buffer_desc.size         = datasize / attribsize
        buffer_desc.attribs      = attribs
        buffer_desc.label        = buffer_name
        buffs.vbuf = buffer_desc
    end

    if(buffers.indices) then
        if(#buffers.indices > 0) then 
            local buffer_desc           = {}
            buffer_desc.type         = "INDEXBUFFER"
            buffer_desc.buffer       = my_buffer
            buffer_desc.size         = buffers.icount
            buffer_desc.label        = name.."-indices"
            buffs.ibuf = buffer_desc
        else 
            return nil         
        end
    end

    if(buffers.storage) then 
        if(#buffers.storage > 0) then 
            local buffer_desc           = {}
            buffer_desc.type         = "STORAGEBUFFER"
            buffer_desc.buffer       = my_buffer
            buffer_desc.size         = buffers.scount
            buffer_desc.label        = name.."-storage"
            buffs.sbuf = buffer_desc     
        else 
            return nil         
        end        
    end

    return buffs
end

-- ----------------------------------------------------------------------------------------
-- Create an image to be used to texture geom or render textures

mesh.image = function(name, rt, width, height, format, samples )
    local img_desc = ffi.new("sg_image_desc[1]")
    img_desc[0].render_target   = rt or false
    img_desc[0].width           = width or 512
    img_desc[0].height          = height or 512
    img_desc[0].pixel_format    = format or sg.SG_PIXELFORMAT_RGBA8
    img_desc[0].sample_count    = samples or 1
    img_desc[0].label           = name
    return sg.sg_make_image(img_desc)
end

-- ----------------------------------------------------------------------------------------
-- Create a material from shader and params
mesh.material  = function(name, shaderfile, params)

    local cached = all_meshes.materials[shaderfile]
    if(cached) then return cached end

    -- local shader    = shc.compile(shaderfile)
    local shd       = {} -- sg.sg_make_shader(shader)

    local sampler_desc      = {}
    sampler_desc.min_filter = "SG_FILTER_LINEAR"
    sampler_desc.mag_filter = "SG_FILTER_LINEAR"
    sampler_desc.wrap_u     = "SG_WRAP_REPEAT"
    sampler_desc.wrap_v     = "SG_WRAP_REPEAT"
    sampler_desc.wrap_w     = "SG_WRAP_REPEAT"  -- only needed for 3D textures
    
    local default_sampler = sampler_desc

    if(shd) then 
        local material = {
            shader          = shd, 
            params          = params,
            name            = name, 
            base_color_smp  = default_sampler,
        }
        all_meshes.materials[shaderfile] = material
        return material
    else 
        print("[Error mesh.material] Could not create material: "..tostring(name))
        return nil 
    end
end

-- ----------------------------------------------------------------------------------------
-- Model makes a pipeline 
mesh.make_mesh = function(name, buffers)

    local mesh = { layout = {}, depth = {}, aabb = buffers.aabb }

    local buffercount = utils.tcount(buffers)
    if(buffercount == 0) then 
        print("[Error mesh.make_mesh] No buffers to process!")
        return nil 
    end
    mesh.layout.buffers = {}
    mesh.layout.attrs   = {}
    -- One attr for each buffer
    for bi, buffer in ipairs(buffers) do
        if(buffer) then 
            mesh.layout.buffers[bi-1] = mesh.layout.buffers[bi-1] or {}
            mesh.layout.buffers[bi-1].stride = buffer.stride * 4
        end

        if(buffer.index_type) then 
            mesh.index_type = buffer.index_type
        end

        if(buffer.cull_mode) then 
            mesh.cullmode = buffer.cullmode
        end

        if(buffer.attrs) then 
            for i, attr in ipairs(buffer.attrs) do
                mesh.layout.attrs[i-1] = mesh.layout.attrs[i-1] or {}
                mesh.layout.attrs[i-1].format = attr.format
                mesh.layout.attrs[i-1].offset = attr.offset
            end
        end

        if(mesh.depth) then 
            mesh.depth = {}
            if(buffer.depth.write_enabled) then 
                mesh.depth.write_enabled = buffer.depth.write_enabled
            end
            if(buffer.depth.compare) then 
                mesh.depth.compare = buffer.depth.compare
            end
        end

        mesh.vbuf = buffer.vbuf
        mesh.ibuf = buffer.ibuf
        mesh.sbuf = buffer.sbuf    
    end

    return mesh
end

-- ----------------------------------------------------------------------------------------
-- Model makes a pipeline 
mesh.model     = function(name, prim, mesh, material)

    -- Stores material with mesh - this should be an index
    -- mesh.material = material

    local pipe_desc = {}
    pipe_desc.layout = pipe_desc.layout or {}
    pipe_desc.depth = pipe_desc.depth or {}
    pipe_desc.layout.buffers         = mesh.layout.buffers
    pipe_desc.layout.attrs           = mesh.layout.attrs
    pipe_desc.shader                 = material.shader 

    if(mesh.index_type) then pipe_desc.index_type = mesh.index_type end
    if(mesh.cull_mode) then pipe_desc.cull_mode = mesh.cull_mode end
    if(mesh.depth) then 
        if(mesh.depth.write_enabled) then pipe_desc.depth.write_enabled = mesh.depth.write_enabled end
        if(mesh.depth.compare) then pipe_desc.depth.compare = mesh.depth.compare end
    end

    local prim_mat = cgltf.get_material(model.data, prim.material)    
    if(prim_mat.alpha_mode == cgltf.cgltf_alpha_mode_blend) then 
        pipe_desc.colors = pipe_desc.colors or {}
        pipe_desc.colors.blend = pipe_desc.colors.blend or {}
        pipe_desc.colors.blend.enabled = true
        pipe_desc.colors.blend.src_factor_rgb = "SG_BLENDFACTOR_SRC_ALPHA"
        pipe_desc.colors.blend.dst_factor_rgb = "SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA"
        pipe_desc.colors.blend.src_factor_alpha = "SG_BLENDFACTOR_ONE"
        pipe_desc.colors.blend.dst_factor_alpha = "SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA"
    elseif(prim_mat.alpha_mode == cgltf.cgltf_alpha_mode_mask) then 

    end

    pipe_desc[0].label                  = name.."-pipeline"

    local pipeline = sg.sg_make_pipeline(pipe_desc)
    
    local binding = ffi.new("sg_bindings[1]", {})
    binding[0].vertex_buffers[0]       = mesh.vbuf
    if(mesh.ibuf) then binding[0].index_buffer = mesh.ibuf end

    local mesh_mat = prim.material
    local tex = nil
    if(mesh_mat and mesh_mat.images) then 
        if(mesh_mat.images.base_color ~= nil) then
            local img = mesh_mat.images.base_color.img or sg.sg_make_image(mesh_mat.images.base_color.info)
            binding[0].images[0]   = img
            tex = true 
        end
    end
    if(tex == nil) then 
        binding[0].images[0]   =  imageutils.default_white_image
    end
    if(material.base_color_smp ~= nil) then
        binding[0].samplers[0] = material.base_color_smp  -- the sampler        
    end
    return {
        pip     = pipeline,
        bind    = binding,
        alpha   = { mode = prim.material.alpha_mode, cutoff = prim.material.alpha_cutoff }
    }
end 

-- ----------------------------------------------------------------------------------------

return mesh 

-- ----------------------------------------------------------------------------------------