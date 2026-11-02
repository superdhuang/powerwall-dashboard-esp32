import * as THREE from "three";
import { OBJLoader } from "https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/loaders/OBJLoader.js";
import { sceneNodes } from "./godot_scene_nodes.js";

const project = "../recovered/gdre_project/";
const carDir = "Ego/3_High/";

async function read(path) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path}: HTTP ${response.status}`);
  return response.text();
}

function transformFromGodot(object, value) {
  const v = value.split(",").map(Number);
  new THREE.Matrix4().set(
    v[0], v[3], v[6], v[9],
    v[1], v[4], v[7], v[10],
    v[2], v[5], v[8], v[11],
    0, 0, 0, 1
  ).decompose(object.position, object.quaternion, object.scale);
}

function findPath(root, path) {
  return path.split("/").filter(p => p !== ".").reduce(
    (parent, name) => parent?.children.find(child => child.name === name), root
  );
}

// The exported GLB retains the original textures/PBR materials, while the
// Godot scene carries visibility, overrides, sockets and the root rotation.
function applyScene(root, sceneText, parser, overrides) {
  const resources = new Map([...sceneText.matchAll(
    /\[ext_resource path="([^"]+)"[^\n]* id=(\d+)\]/g
  )].map(m => [m[2], m[1]]));
  for (const {header, properties} of sceneNodes(sceneText)) {
    const name = header.match(/name="([^"]+)"/)[1];
    const parentPath = header.match(/parent="([^"]+)"/)?.[1];
    let object = parentPath === undefined ? root : findPath(root, `${parentPath}/${name}`);
    if (!object && /type="(?:Spatial|Position3D)"/.test(header)) {
      const parent = findPath(root, parentPath);
      if (!parent) throw new Error(`Missing Model 3 parent: ${parentPath}`);
      object = new THREE.Group();
      object.name = name;
      parent.add(object);
    }
    if (!object) continue;
    const transform = properties.match(/^transform = Transform\( ([^\n]+) \)/m);
    if (transform) transformFromGodot(object, transform[1]);
    const visibility = properties.match(/^visible = (true|false)/m);
    if (visibility) object.visible = visibility[1] === "true";
    for (const slot of properties.matchAll(/^material\/(\d+) = ExtResource\( (\d+) \)/gm)) {
      const resource = resources.get(slot[2]);
      const meshes = object.isMesh ? [object] : object.children.filter(c => c.isMesh);
      for (const mesh of meshes) {
        if ((parser.associations.get(mesh)?.primitives ?? 0) !== Number(slot[1])) continue;
        mesh.userData.godotMaterial = resource;
        const replacement = overrides[resource];
        if (replacement) mesh.material = replacement;
      }
    }
  }
}

function useOriginalMRAChannels(material, metallic = true) {
  material.onBeforeCompile = shader => {
    shader.fragmentShader = shader.fragmentShader.replace(
      "texture2D( aoMap, vAoMapUv ).r", "texture2D( aoMap, vAoMapUv ).b"
    );
    if (metallic) shader.fragmentShader = shader.fragmentShader.replace(
      "metalnessFactor *= texelMetalness.b;", "metalnessFactor *= texelMetalness.r;"
    );
  };
  material.customProgramCacheKey = () => `godot-mra-${metallic}`;
  return material;
}

function originalNodeTransform(text, name, parent) {
  for (const {header, properties} of sceneNodes(text)) {
    if (header.match(/name="([^"]+)"/)?.[1] !== name ||
        header.match(/parent="([^"]+)"/)?.[1] !== parent) continue;
    const value = properties.match(/^transform = Transform\( ([^\n]+) \)/m)?.[1];
    if (value) return value;
  }
  throw new Error(`Missing source transform: ${parent}/${name}`);
}

export async function loadRedModel3Performance(loader, renderer) {
  const textureLoader = new THREE.TextureLoader();
  const texture = async (path, color = false) => {
    const result = await textureLoader.loadAsync(project + path);
    result.flipY = false;
    if (color) result.colorSpace = THREE.SRGBColorSpace;
    return result;
  };
  const [gltf, sceneText, wheelText, exteriorMRA, groundAO,
    wheelMRA, tireBC, tireRA, panorama, brakeBC, brakeMRA,
    brakeMeshes, seatBlack, seatAO, houseScene, garageScene] = await Promise.all([
    loader.loadAsync(project + carDir + "Model3_High.glb"),
    read(project + carDir + "Model3_High.tscn"),
    read(project + "Ego/Wheels/Objects/Wheel_Uberturbine.obj"),
    texture(carDir + "Textures/Exterior_MRA.png"),
    texture(carDir + "Textures/Ground_AO.png", true),
    texture("Ego/Wheels/Textures/Uberturbine_MRA.png"),
    texture("Ego/Wheels/Textures/Tire_BC.png", true),
    texture("Ego/Wheels/Textures/Tire_RA.png"),
    // mobile.tscn assigns this same panorama to both Energy themes.
    texture("Energy/Environment/VehicleGarageUpdated.png", true),
    texture("Ego/Brakes/Textures/Brakes_Perf_BC.png", true),
    texture("Ego/Brakes/Textures/Brakes_Perf_MRA.png"),
    Promise.all(["Caliper_Perf_F", "Rotor_Perf_F", "Caliper_Perf_R", "Rotor_Perf_R"]
      .map(name => read(project + `Ego/Brakes/Objects/${name}.obj`))),
    texture(carDir + "Textures/Black.png", true),
    texture(carDir + "Textures/Seats_AO.png"),
    read(project + "Energy/Load/Residential/Classic/ResidentialClassic.tscn"),
    read(project + "Energy/Vehicle/VehicleGarage.tscn")
  ]);
  panorama.mapping = THREE.EquirectangularReflectionMapping;
  const pmrem = new THREE.PMREMGenerator(renderer);
  const environment = pmrem.fromEquirectangular(panorama).texture;
  pmrem.dispose();
  // VehicleOptions.ExteriorColorValue.RedMulticoat, applied by Vehicle.gd.
  const red = new THREE.Color(10 / 255, 1 / 255, 1 / 255);
  const paint = useOriginalMRAChannels(new THREE.MeshStandardMaterial({
    name: "PaintSkybox-RedMulticoat", color: red, metalness: 0.1,
    roughness: 0.04, aoMap: exteriorMRA
  }), false);
  paint.aoMap.channel = 0;
  const roughPaint = paint.clone();
  roughPaint.name = "PaintRough-RedMulticoat";
  roughPaint.roughness = 1;
  useOriginalMRAChannels(roughPaint, false);
  const ground = new THREE.MeshBasicMaterial({
    map: groundAO, color: 0x1b1b1b, transparent: true, depthWrite: false
  });
  seatAO.channel = 0;
  const seats = new THREE.MeshStandardMaterial({
    name: "InteriorSeats", map: seatBlack, color: 0x141414,
    roughness: 0.8, aoMap: seatAO
  });
  const root = gltf.scene.children[0];
  applyScene(root, sceneText, gltf.parser, {
    "res://Ego/3_High/PaintSkybox.material": paint,
    "res://Ego/3_High/Paint_Rough.material": roughPaint,
    "res://Ego/3_High/Ground_Plane.material": ground,
    "res://Ego/3_High/Interior_Seats_Black.material": seats
  });
  // User override: remove reflections/highlights from the original roof-glass
  // assembly only. Clone its materials so side windows and paint stay intact.
  const roofGlass = root.getObjectByName("Windows_Top");
  if (!roofGlass) throw new Error("Missing original Model 3 roof glass");
  roofGlass.traverse(mesh => {
    if (!mesh.isMesh) return;
    const withoutReflection = original => {
      if (!original.isMeshStandardMaterial) return original;
      const material = new THREE.MeshPhysicalMaterial();
      THREE.MeshStandardMaterial.prototype.copy.call(material, original);
      // Standard.copy resets defines; restore PHYSICAL so the shader honors
      // specularIntensity=0 instead of using Standard's fixed glass specular.
      material.defines = { STANDARD: "", PHYSICAL: "" };
      material.name = original.name + "-RoofNoReflection";
      material.specularIntensity = 0;
      material.userData.noReflection = true;
      return material;
    };
    mesh.material = Array.isArray(mesh.material)
      ? mesh.material.map(withoutReflection) : withoutReflection(mesh.material);
  });

  // VehicleOptions: UberTurbine20Gunpowder -> Wheel_Uberturbine.tscn.
  const wheelMaterial = useOriginalMRAChannels(new THREE.MeshStandardMaterial({
    name: "Performance Uberturbine rim", color: 0x0a0a0a, metalness: 0.85, roughness: 0.7,
    metalnessMap: wheelMRA, roughnessMap: wheelMRA, aoMap: wheelMRA
  }));
  const tireMaterial = useOriginalMRAChannels(new THREE.MeshPhysicalMaterial({
    name: "Original tire", map: tireBC, roughness: 0.75, roughnessMap: tireRA, aoMap: tireRA,
    aoMapIntensity: 0.7, specularIntensity: 0
  }), false);
  wheelMRA.channel = 0;
  tireRA.channel = 0;
  const wheel = new OBJLoader().parse(wheelText);
  wheel.traverse(mesh => {
    if (!mesh.isMesh) return;
    const replace = material => material.name === "Uberturbine" ? wheelMaterial : tireMaterial;
    mesh.material = Array.isArray(mesh.material) ? mesh.material.map(replace) : replace(mesh.material);
  });
  const wheelInstances = [];
  for (const name of ["Wheel_LF_Spatial", "Wheel_LR_Spatial", "Wheel_RF_Spatial", "Wheel_RR_Spatial"]) {
    const socket = findPath(root, `Spatials/${name}`);
    if (!socket) throw new Error(`Missing original Model 3 wheel socket: ${name}`);
    const instance = wheel.clone(true);
    instance.name = `${name}_Uberturbine20_WithTire`;
    socket.add(instance);
    wheelInstances.push(instance);
  }
  const brakeMaterial = useOriginalMRAChannels(new THREE.MeshStandardMaterial({
    map: brakeBC, color: 0x3c3c3c, metalness: 0.6, roughness: 0,
    metalnessMap: brakeMRA, roughnessMap: brakeMRA, aoMap: brakeMRA
  }));
  brakeMRA.channel = 0;
  for (const [name, indices] of [
    ["Brakes_LF_Spatial", [0, 1]], ["Brakes_LR_Spatial", [2, 3]],
    ["Brakes_RF_Spatial", [0, 1]], ["Brakes_RR_Spatial", [2, 3]]
  ]) {
    const socket = findPath(root, `Spatials/${name}`);
    if (!socket) throw new Error(`Missing original Model 3 brake socket: ${name}`);
    for (const index of indices) {
      const brake = new OBJLoader().parse(brakeMeshes[index]);
      brake.traverse(mesh => { if (mesh.isMesh) mesh.material = brakeMaterial; });
      socket.add(brake);
    }
  }
  // The original charge-cap animation's open endpoint.
  const cap = findPath(root, "Charge_Cap_Spatial");
  if (cap) cap.rotation.set(THREE.MathUtils.degToRad(88.6351), THREE.MathUtils.degToRad(16.544), 0, "YXZ");
  const chargePort = findPath(root, "Spatials/ChargePort");
  if (!chargePort) throw new Error("Missing original Model 3 charge port");
  const spoiler = findPath(root, "Trunk_Bracket_Main_Spatial/Trunk_Spatial/Spoiler1");
  if (!spoiler) throw new Error("Missing original Model 3 Performance spoiler");
  spoiler.visible = true;
  gltf.scene.traverse(mesh => {
    if (!mesh.isMesh) return;
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    for (const material of (Array.isArray(mesh.material) ? mesh.material : [mesh.material])) {
      if (!material.isMeshStandardMaterial) continue;
      // User override: disable both panorama reflection and direct specular
      // on rubber and roof glass; retain original textures and other materials.
      const noReflection = material === tireMaterial || material.userData.noReflection;
      material.envMap = noReflection ? null : environment;
      // env/mobile.tres's baseline ambient energy. Runtime app overrides
      // depend on SET_ENV_PARAMS and are not captured in this offline view.
      material.envMapIntensity = noReflection ? 0 : 2.5;
      material.needsUpdate = true;
    }
  });
  // Keep the original garage anchor and nested switcher scale verbatim.
  const garageAnchor = new THREE.Group();
  garageAnchor.name = "Vehicle";
  transformFromGodot(garageAnchor, originalNodeTransform(houseScene, "Vehicle", "."));
  const switcher = new THREE.Group();
  switcher.name = "VehicleGarageManager_Vehicle";
  transformFromGodot(switcher, originalNodeTransform(garageScene, "Vehicle", "."));
  garageAnchor.add(switcher);
  const parkingHeading = new THREE.Group();
  parkingHeading.name = "UserRequested_NoseIn";
  // Explicit user adjustment: nose into garage. No translation/scale changes.
  parkingHeading.rotation.y = Math.PI;
  switcher.add(parkingHeading);
  parkingHeading.add(gltf.scene);
  garageAnchor.updateMatrixWorld(true);
  for (const instance of wheelInstances) {
    const materials = new Set();
    instance.traverse(mesh => {
      if (mesh.isMesh) for (const m of (Array.isArray(mesh.material) ? mesh.material : [mesh.material])) materials.add(m);
    });
    if (!materials.has(wheelMaterial) || !materials.has(tireMaterial)) {
      throw new Error(`Incomplete rim/tire assembly: ${instance.name}`);
    }
  }
  console.info("Model 3 Performance wheel assemblies " + JSON.stringify(wheelInstances.map(wheel => {
    const bounds = new THREE.Box3().setFromObject(wheel);
    return {name: wheel.name, min: bounds.min.toArray(), max: bounds.max.toArray()};
  })));
  return { model: garageAnchor, chargePort };
}
