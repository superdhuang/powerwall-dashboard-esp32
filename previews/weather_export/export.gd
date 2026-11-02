extends SceneTree

func _init():
	var source = "../../recovered/gdre_project/Energy/Load/Residential/Weather/"
	var mesh = load(source + "LongWind.mesh")
	var arrays = mesh.surface_get_arrays(0)
	var vertices = arrays[Mesh.ARRAY_VERTEX]
	var uvs = arrays[Mesh.ARRAY_TEX_UV]
	var indices = arrays[Mesh.ARRAY_INDEX]
	var f = File.new()
	f.open("res://long_wind.json", File.WRITE)
	var v = []
	var uv = []
	for p in vertices:
		v.append(p.x)
		v.append(p.y)
		v.append(p.z)
	for p in uvs:
		uv.append(p.x)
		uv.append(p.y)
	f.store_string(to_json({"positions":v,"uvs":uv,"indices":Array(indices)}))
	f.close()
	var plane_arrays = PlaneMesh.new().get_mesh_arrays()
	print("GODOT_PLANE_POSITIONS ", plane_arrays[Mesh.ARRAY_VERTEX])
	print("GODOT_PLANE_UV ", plane_arrays[Mesh.ARRAY_TEX_UV])
	var check = str2var("Transform( 3.11679, 2.31033, -3.76402e-07, 0, -5.85576e-07, -3.59424, -2.6153, 2.75335, -4.48577e-07, -0.00353467, 1.01103, -0.0430779 )")
	print("GODOT_SKY_BASIS x=", check.basis.x, " y=", check.basis.y, " z=", check.basis.z)
	var frost = load(source + "Frost.tres")
	var noise = frost.get_shader_param("noise_texture")
	# NoiseTexture generation is deferred by Godot until the next idle frames.
	yield(self, "idle_frame")
	yield(self, "idle_frame")
	var image = noise.get_data()
	if image != null:
		image.save_png("res://frost_noise.png")
	print("ORIGINAL_WEATHER_EXPORT_COMPLETE vertices=", vertices.size())
	quit()
