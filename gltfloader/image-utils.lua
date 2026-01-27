-------------------------------------------------------------------------------------------------

local utils = require("gltfloader.utils")

local imageutils = {
	ctr 		= 0,
	images		= 	{}
}

-------------------------------------------------------------------------------------------------

local function image_id()
	imageutils.ctr = imageutils.ctr + 1
	return imageutils.ctr
end

-------------------------------------------------------------------------------------------------

local function make_defaults()

	-- a single white RGBA pixel
	local white_pixel = ffi.new("uint32_t[1]", 0xFFFFFFFF)  -- 0xAARRGGBB = white

	-- describe the image
	local img_desc = ffi.new("sg_image_desc")
	img_desc.width  = 1
	img_desc.height = 1
	img_desc.pixel_format = sg.SG_PIXELFORMAT_RGBA8
	img_desc.data.subimage[0][0].ptr  = white_pixel
	img_desc.data.subimage[0][0].size = ffi.sizeof(white_pixel)

	-- create the sokol image
	imageutils.default_white_image = sg.sg_make_image(img_desc)
end

-------------------------------------------------------------------------------------------------

local function loadimage(imgname, imagefilepath, tid )
	
	if(imagefilepath == nil) then 
		print("[Image Load Error] imagefilepath is nil.") 
		return nil
	end

	imagefilepath = utils.cleanstring( imagefilepath )
	local buff = sys.load_buffer("/"..imagefilepath)
	local data_str = buffer.get_bytes(buff, "data")
	local img = image.load(data_str, {})
	if(img == nil) then 
		print("[Image Load Error] Cannot load image: "..imagefilepath) 
		return nil
	end 
	pprint("[Info] Image loaded: "..imagefilepath.."  size: "..#img.buffer)
	local res = {
		id 		= tid,
		img 	= img, 
		name 	= imgname,
		src 	= imagefilepath,
	}
	imageutils.images[tid] = res

	return res
end 

-------------------------------------------------------------------------------------------------

local function loadimagebuffer(imgname, buf, bufsize, tid )

	if(buf == nil) then 
		print("[Image Load Error] imagebuffer is nil.") 
		return nil
	end
	
	local img = image.load(buf, {})
	if(img == nil) then 
		print("[Image Load Error] Cannot load image buffer from gameobj: "..goname) 
		return nil
	end 
	
	local res = {
		id 		= tid,
		img 	= img, 
		name 	= imgname,
	}
	imageutils.images[tid] = res

	return res
end 

-------------------------------------------------------------------------------------------------

imageutils.make_defaults 	= make_defaults
imageutils.loadimage 		= loadimage
imageutils.loadimagebuffer 	= loadimagebuffer
imageutils.image_id			= image_id

-------------------------------------------------------------------------------------------------

return imageutils

-------------------------------------------------------------------------------------------------