embedded_components {
  id: "mesh"
  type: "mesh"
  data: "material: \"/builtins/materials/model.material\"\n"
  "vertices: \"/example/mesh_base.buffer\"\n"
  "textures: \"/builtins/assets/images/logo/logo_blue_256.png\"\n"
  "position_stream: \"position\"\n"
  ""
}
embedded_components {
  id: "model"
  type: "model"
  data: "mesh: \"/builtins/assets/meshes/cube.dae\"\n"
  "name: \"{{NAME}}\"\n"
  "materials {\n"
  "  name: \"default\"\n"
  "  material: \"/builtins/materials/model.material\"\n"
  "  textures {\n"
  "    sampler: \"tex0\"\n"
  "    texture: \"/builtins/assets/images/logo/logo_256.png\"\n"
  "  }\n"
  "}\n"
  ""
  position {
    x: 2.0
  }
}
