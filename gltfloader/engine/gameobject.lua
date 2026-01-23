local tinsert 	= table.insert
local utils 	= require("gltfloader.utils")
-- local hmm       = require("hmm")

local geom 		= require("gltfloader.geometry-utils")

-- --------------------------------------------------------------------------------------------------------
-- Internal storage for a mesh gameobject
--
--  Note: components allowed are currently 16. I think if you are adding more than that, then you 
--        have a poor design. This needs to be simple and clean. I may reduce to 12 or less.

local COMPONENTS_MAX 		= 10

local gameobj_struct =  {
		id = 0,
		name = "",
		pos = vmath.vector3(),
		rot = vmath.quat(),
		scale = vmath.vector3(),

		components = {},
		sibling 		= 0,
		parent 			= 0,
} 

-- --------------------------------------------------------------------------------------------------------

local GAMEOBJECTS_MAX	= 1000		-- Conservative alloc. This will be about 96K mem wise.
local GAMEOBJECTS_REALLOC_THRESHOLD 	= 0.9	-- 90% hit, then realloc

-- --------------------------------------------------------------------------------------------------------

local gameobject = {

	-- All gameobjects are stored in a memory block. 
	all_gameobjects = {},
	-- This count is important - if it reached 90% it does a realloc and doubles and copies gameobjects
	alloc_count 	= GAMEOBJECTS_MAX,
	count 			= 0, 

	lookup_ids		= {},
	lookup_names	= {},	-- Note name lookup can be messy. Esp if duplicate names are used
}

-- --------------------------------------------------------------------------------------------------------
-- Clears the gameobject array
--   We dont reset the alloc_count, incase the user wants to refill. Faster to leave it. 
gameobject.reset = function( )

	gameobject.count = 0 
	gameobject.all_gameobjects =  {}
	gameobject.lookup_ids		= {}
	gameobject.lookup_names		= {}

end

-- --------------------------------------------------------------------------------------------------------

gameobject.check = function( )

	if(gameobject.count / gameobject.alloc_count > GAMEOBJECTS_REALLOC_THRESHOLD) then 

		gameobject.alloc_count = gameobject.alloc_count * 2
		for i=0, gameobject.alloc_count-1 do 
			gameobject.all_gameobjects[i] = gameobject.all_gameobjects[i] or utils.deepcopy(gameobj_struct)
		end
	end
end

-- --------------------------------------------------------------------------------------------------------

gameobject.create = function( id, name, pos, rot, scale, parent )

	-- Sometimes the original gameobject name is used
	name = tostring(name)

	pos = pos or vmath.vector3(0, -math.huge, 0) -- "hides the object" by moving it vertically a long way down.
	rot = rot or vmath.quat(0, 0, 0, 0)
	scale = scale or vmath.vector3(1, 1, 1)

	gameobject.check()
	local gobj_index = gameobject.count 
	local gobj = gameobject.all_gameobjects[gobj_index] or utils.deepcopy(gameobj_struct)

	gobj.id 	= id or gobj_index
	gobj.name 	= tostring(name or "dummy")
	gobj.pos 	= pos 
	gobj.rot 	= rot 
	gobj.scale 	= scale 
	gobj.parent = parent or 0		-- Must be an index!!!

	gameobject.all_gameobjects[gobj_index] = gobj

	-- Reverse lookups - will be useful if needed. May add spatial as well.
	gameobject.lookup_ids[gobj.id]		= gobj_index
	gameobject.lookup_names[gobj.name]	= gobj_index
	gameobject.count = gameobject.count + 1
	return gobj_index
end 

-- --------------------------------------------------------------------------------------------------------

gameobject.get_go = function( id )
	assert(id < gameobject.count and id >= 0)
	return gameobject.all_gameobjects[id]
end

-- --------------------------------------------------------------------------------------------------------

gameobject.goname = function( id )
	assert(id < gameobject.count and id >= 0)
	return tostring(gameobject.all_gameobjects[id].name)
end

-- --------------------------------------------------------------------------------------------------------

gameobject.set_position = function( id, pos )
	assert(id < gameobject.count and id >= 0)
	gameobject.all_gameobjects[id].pos = pos
end

-- --------------------------------------------------------------------------------------------------------

gameobject.set_rotation = function( id, rot )
	assert(id < gameobject.count and id >= 0)
	gameobject.all_gameobjects[id].rot = rot
end

-- --------------------------------------------------------------------------------------------------------

gameobject.set_scale = function( id, scale )
	assert(id < gameobject.count and id >= 0)
	gameobject.all_gameobjects[id].scale = scale
end

-- --------------------------------------------------------------------------------------------------------

gameobject.set_parent = function( id, parent )
	assert(id < gameobject.count and id >= 0)
	assert(parent < gameobject.count and parent >= 0)
	gameobject.all_gameobjects[id].parent = parent
end

-- --------------------------------------------------------------------------------------------------------

gameobject.get_mesh = function(id)
	assert(id < gameobject.count and id >= 0)
	local go = gameobject.all_gameobjects[id]
	return geom:GetMesh(tostring(go.name))
end 

-- --------------------------------------------------------------------------------------------------------

return gameobject 

-- --------------------------------------------------------------------------------------------------------
