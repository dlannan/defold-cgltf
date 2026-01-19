# defold-cgltf
CGLTF loader extension for Defold, that loads in gltf and glb files at runtime.

## Overview

The loader is being built to be able to load gltf/glb files at runtime (esp in the web). 

A dev diary is being kept here for comments, ideas or issues:

https://forum.defold.com/t/cgltf-loader-extension/82250

## Usage

The api is intended to be as simple as possible. There are two main portions of the API:
- The C++ cgltf calls like open_file, open_memory and close.
- The Defold lua api side that creates meshes, materials etc. 

The general use will be (to be updated on completion):

Load a file into memory using sys.load_buffer or similar.
Process the gltf passing in your provided material and other options.
The returned gameobject uri can then be used as per normal in Defold.

Note: Animation is not yet considered, and may need some special operations to work cleanly.