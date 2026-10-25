vec3 unpack_specular_color() { return unpack_rgb10(push_constants.material_ext.x); }

vec3 unpack_attenuation_color() { return unpack_rgb10(push_constants.material_ext.y); }

vec3 unpack_sheen_color() { return unpack_rgb10(push_constants.material_ext.z); }
