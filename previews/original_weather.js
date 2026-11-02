import * as THREE from 'three';
import {sceneNodes} from './godot_scene_nodes.js';
import {weatherInput, weatherFactors, remap} from './weather_presets.js';
import {numericValues as nums,transformRows} from './godot_resource_values.js';

const root='../recovered/gdre_project/';
const dir='Energy/Load/Residential/Weather/';
const clamp=THREE.MathUtils.clamp;
const textCache=new Map(), textureCache=new Map();
async function read(path) {
  if(!textCache.has(path)) textCache.set(path,fetch(root+path).then(r=>{if(!r.ok)throw Error(path);return r.text();}));
  return textCache.get(path);
}
function prop(s,k) {return s.match(new RegExp('^'+k.replaceAll('/','\\/')+' = (.*)$','m'))?.[1];}
function transform(node,s) {
  const raw=prop(s,'transform'); if(!raw)return;
  node.matrix.set(...transformRows(nums(raw)));
  node.matrix.decompose(node.position,node.quaternion,node.scale);
  node.matrixAutoUpdate=false;
}
function blocks(text) {
  return [...text.matchAll(/^\[(sub_resource|ext_resource|resource)([^\]]*)\]\n([\s\S]*?)(?=^\[|$(?![\s\S]))/gm)];
}
function block(text,id) {return blocks(text).find(b=>b[1]==='sub_resource'&&b[2].match(/id=(\d+)/)?.[1]===String(id))?.[3];}
function embedded(text) {
  const code=text.match(/^code = "([\s\S]*?)"\s*$/m)?.[1];
  return code?.replace(/\\n/g,'\n').replace(/\\t/g,'\t').replace(/\\"/g,'"');
}
function ext(text,id) {return blocks(text).find(b=>b[1]==='ext_resource'&&b[2].match(/id=(\d+)/)?.[1]===String(id))?.[2].match(/path="res:\/\/([^"]+)"/)?.[1];}
function reference(v){return v?.match(/(Ext|Sub)Resource\(\s*(\d+)\s*\)/);}
async function tex(path) {
  if(!textureCache.has(path)) textureCache.set(path,new THREE.TextureLoader().loadAsync(path.startsWith('../')?path:root+path).then(t=>{
    // Original project.godot selects GLES2 (OUTPUT_IS_SRGB): weather shaders
    // sample/output display-space colors directly, without extra conversion.
    t.flipY=false;t.colorSpace=THREE.NoColorSpace;return t;
  }));
  return textureCache.get(path);
}
function removeFunction(s,name) {
  const start=s.indexOf('void '+name+'(');if(start<0)return s;
  let i=s.indexOf('{',start),depth=1,end=i+1;
  for(;depth&&end<s.length;end++){if(s[end]==='{')depth++;if(s[end]==='}')depth--;}
  return s.slice(0,start)+s.slice(end);
}
const plane=new THREE.PlaneGeometry(2,2).rotateX(-Math.PI/2);
// Godot3.2 PlaneMesh export: at z=+1 UV.y=1; at z=-1 UV.y=0.
for(let i=0;i<plane.attributes.uv.count;i++)plane.attributes.uv.setY(i,1-plane.attributes.uv.getY(i));
const white=new THREE.DataTexture(new Uint8Array([255,255,255,255]),1,1);white.needsUpdate=true;

// This adapter reads shader code/parameters from the recovered resources.
// It translates only the unshaded weather subset, not a general Godot renderer.
async function shaderMaterial(source,parameters,context,{particle=false}={}) {
  const uniforms={currentTime:{value:0}};
  for(const m of source.matchAll(/uniform\s+(?:(?:highp|mediump|lowp)\s+)?(\w+)\s+(\w+)([^;]*);/g)) {
    const [,,name,tail]=m,type=m[1];
    let raw=prop(parameters,'shader_param/'+name)??tail.match(/=\s*(.*)/)?.[1];
    let value=0;
    if(type==='sampler2D') {
      const ref=reference(raw); value=white;
      if(ref?.[1]==='Ext')value=await tex(ext(context,ref[2]));
      else if(name==='noise_texture'){
        value=await tex('../previews/weather_export/frost_noise.png');value.colorSpace=THREE.NoColorSpace;
      }
    }else if(type==='bool')value=raw==='true';
    else if(type==='vec4')value=new THREE.Vector4(...(raw?nums(raw):[0,0,0,0]));
    else if(type==='vec3')value=new THREE.Vector3(...(raw?nums(raw):[0,0,0]));
    else if(type==='vec2')value=new THREE.Vector2(...(raw?nums(raw):[0,0]));
    else value=Number(raw??0);
    uniforms[name]={value};
  }
  const vertexBody=source.match(/void vertex\(\)\s*\{([\s\S]*?)\n\}/)?.[1]??'';
  let fragment=removeFunction(source,'vertex')
    .replace(/^shader_type[^;]+;/gm,'').replace(/^render_mode[^;]+;/gm,'')
    .replace(/(uniform\s+(?:(?:highp|mediump|lowp)\s+)?\w+\s+\w+)[^;]*;/g,'$1;')
    .replace(/void fragment\(\)/,'void main()')
    .replace(/\btexture\(/g,'texture2D(').replace(/\bUV\b/g,'vUv').replace(/\bTIME\b/g,'currentTime')
    .replace(/\bCOLOR\b/g,'particleColor')
    // Godot accepts integer bounds for float clamp; WebGL GLSL does not.
    .replace(/, 0, 1\)/g,', 0.0, 1.0)');
  fragment=fragment.replace(/}\s*$/,`gl_FragColor=vec4(ALBEDO,clamp(ALPHA,0.0,1.0))${particle&&!source.includes('COLOR')?'*particleColor.a':''};
  }`);
  fragment='varying vec2 vUv; varying vec4 particleColor; uniform float currentTime; vec3 ALBEDO; float ALPHA=1.0; float METALLIC; float ROUGHNESS; float SPECULAR;\n'+fragment;
  const wave=vertexBody.includes('waveX')?vertexBody.replace(/UV=UV\*uv1_scale.xy\+uv1_offset.xy;/,'').replace(/\bUV\b/g,'vUv').replace(/\bTIME\b/g,'currentTime').replace(/VERTEX/g,'p'):'';
  const vertex=`varying vec2 vUv; varying vec4 particleColor; varying vec3 world_position;
    uniform float currentTime; uniform vec3 uv1_scale; uniform vec3 uv1_offset;
    ${wave?'const float TAU=6.28318530718;'+[...source.matchAll(/uniform float (wave_\w+)[^;]*;/g)].map(m=>'uniform float '+m[1]+';').join('\n'):''}
    ${particle?'attribute float instanceAlpha;':''}
    void main(){vUv=uv*uv1_scale.xy+uv1_offset.xy;
      vec3 p=position; ${wave}
      particleColor=vec4(1.0,1.0,1.0,${particle?'instanceAlpha':'1.0'});
      vec4 local=vec4(p,1.0);
      #ifdef USE_INSTANCING
      local=instanceMatrix*local;
      #endif
      world_position=(modelMatrix*local).xyz;
      gl_Position=projectionMatrix*modelViewMatrix*local;}`;
  uniforms.uv1_scale??={value:new THREE.Vector3(1,1,1)};
  uniforms.uv1_offset??={value:new THREE.Vector3()};
  const material=new THREE.ShaderMaterial({uniforms,vertexShader:vertex,fragmentShader:fragment,
    // Orbit preview can view the authored sky cards from behind. Render both
    // sides without moving or rotating any of the original scene nodes.
    transparent:true,depthWrite:false,side:THREE.DoubleSide,
    blending:source.includes('blend_add')?THREE.AdditiveBlending:THREE.NormalBlending});
  material.userData.source=context;
  material.userData.renderPriority=Number(prop(parameters,'render_priority')||0);
  return material;
}
async function materialFrom(text,raw,particle=false) {
  const ref=reference(raw);if(!ref)throw Error('Missing original material '+raw);
  let params,source,context=text;
  if(ref[1]==='Ext'){context=await read(ext(text,ref[2]));params=context;}
  else params=block(text,ref[2]);
  const shaderRef=reference(prop(params,'shader'));
  if(shaderRef?.[1]==='Ext')source=await read(ext(context,shaderRef[2]));
  else if(shaderRef)source=embedded(block(context,shaderRef[2]));
  else {
    // Godot's unshaded SpatialMaterial used by rain drops/splashes.
    source=`uniform vec4 albedo; uniform sampler2D texture_albedo;
      void fragment(){vec4 t=texture(texture_albedo,UV);ALBEDO=albedo.rgb*t.rgb;ALPHA=albedo.a*t.a;}`;
    params=params.replace(/^albedo_color = /m,'shader_param/albedo = ').replace(/^albedo_texture = /m,'shader_param/texture_albedo = ');
  }
  return shaderMaterial(source,params,context,{particle});
}

export async function loadOriginalWeather({scene,loader}) {
  const materials=[],emitters=[],nodes=new Map();
  const weatherRoot=new THREE.Group();weatherRoot.name='OriginalWeather';scene.add(weatherRoot);
  const classic=await read('Energy/Load/Residential/Classic/ResidentialClassic.tscn');
  transform(weatherRoot,[...sceneNodes(classic)].find(n=>n.header.startsWith('name="Sky" ')).properties);
  const pointcloud=[...sceneNodes(classic)].find(n=>n.header.startsWith('name="PointcloudWallConnector" '));
  // IntroAnimation.gd's Classic-site runtime scale adjustment. This scene
  // explicitly has PointcloudWallConnector.enabled=false.
  if(prop(pointcloud?.properties||'','enabled')==='false'){
    weatherRoot.scale.setScalar(1.42);weatherRoot.updateMatrix();
  }
  const sky=await read(dir+'SkyGradient.tscn');
  async function loadNodes(text,parent,map) {
    for(const n of sceneNodes(text)) {
      const name=n.header.match(/name="([^"]+)"/)[1],type=n.header.match(/type="([^"]+)"/)?.[1];
      const p=n.header.match(/parent="([^"]+)"/)?.[1];
      if(p===undefined){map.set('.',parent);continue;}
      const path=p==='.'?name:p+'/'+name;
      if(type==='Tween'||type==='AnimationPlayer'||type==='Node')continue;
      let node=new THREE.Group();
      if(type==='MeshInstance'&&!path.includes('CloudsShader')){
        const material=await materialFrom(text,prop(n.properties,'material/0'));
        materials.push(material);
        let geometry=plane;
        if(name==='LightningBolt') {
          const gltf=await loader.loadAsync(root+dir+'Lightning.glb');
          gltf.scene.traverse(o=>{if(o.isMesh)geometry=o.geometry;});
        }
        node=new THREE.Mesh(geometry,material);
        node.renderOrder=material.userData.renderPriority;
        // Sky cards are a backdrop, not surfaces that can intersect the house.
        // Keep authored transforms but composite them in the background pass.
        node.layers.set(1);
      }
      if(type==='CPUParticles')node=await particles(text,n.properties,name);
      transform(node,n.properties);node.name=name;
      if(prop(n.properties,'visible')==='false')node.visible=false;
      (map.get(p)||parent).add(node);map.set(path,node);
    }
  }
  async function particles(text,s,name,geometry=plane) {
    const material=await materialFrom(text,prop(s,'material_override'),true);materials.push(material);
    const max=name==='Drops'?500:name.includes('Splash')?50:name==='Wind'?33:Number(prop(s,'amount')||50);
    geometry=geometry.clone();geometry.setAttribute('instanceAlpha',new THREE.InstancedBufferAttribute(new Float32Array(max).fill(1),1));
    const mesh=new THREE.InstancedMesh(geometry,material,max);mesh.frustumCulled=false;
    mesh.renderOrder=material.userData.renderPriority;
    if(name==='Fog')mesh.layers.set(1);
    const conf={mesh,name,max,life:Number(prop(s,'lifetime')||1),extent:nums(prop(s,'emission_box_extents')||'0,0,0'),
      gravity:nums(prop(s,'gravity')||'0,-9.8,0'),velocity:Number(prop(s,'initial_velocity')||0),
      scale:Number(prop(s,'scale_amount')||1),random:Number(prop(s,'scale_amount_random')||0),
      colorAlpha:nums(prop(s,'color')||'1,1,1,1')[3],ramp:!!prop(s,'color_ramp'),
      seeds:Array.from({length:max},(_,i)=>[...Array(5)].map((_,j)=>THREE.MathUtils.seededRandom(i*19+j*37+51)))};
    emitters.push(conf);return mesh;
  }
  await loadNodes(sky,weatherRoot,nodes);
  for(const name of ['Snow','Wind']) {
    const text=await read(dir+name+'.tscn'),map=new Map(),parent=nodes.get(name);
    const rootNode=[...sceneNodes(text)][0];transform(parent,rootNode.properties);
    if(name==='Snow')await loadNodes(text,parent,map);
    else {
      const data=await fetch(new URL('./weather_export/long_wind.json',import.meta.url)).then(r=>r.json());
      const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(data.positions,3));
      g.setAttribute('uv',new THREE.Float32BufferAttribute(data.uvs,2));g.setIndex(data.indices);
      const mesh=await particles(text,rootNode.properties,'Wind',g);parent.add(mesh);
    }
  }
  const rainRoot=new THREE.Group();rainRoot.name='OriginalClassicRain';scene.add(rainRoot);
  const rainText=await read(dir+'ResClassicRain.tscn');transform(rainRoot,[...sceneNodes(rainText)][0].properties);
  await loadNodes(rainText,rainRoot,new Map());
  const frostMaterial=await materialFrom(await read('Energy/Load/Residential/Classic/SolarPanel.tscn'),'ExtResource(9)');materials.push(frostMaterial);
  let state=weatherInput({id:'clear'}),factors=weatherFactors(state),lastSwitch=0;
  const originalBackground=scene.background.clone();
  const set=(node,k,v)=>{if(node?.material?.uniforms[k])node.material.uniforms[k].value=v;};
  function apply(preset){
    state=weatherInput(preset);factors=weatherFactors(state);lastSwitch=performance.now()/1000;
    const gradient=nodes.get('SkyGradient');
    // IntroAnimation.gd hides the day gradient at night (moon/stars stay).
    set(gradient,'alpha',state.is_night?0:1);
    const time=state.time_of_day;
    const day=time>85?clamp(remap(time,85,90,1,0),0,1):time<15?clamp(remap(time,10,15,0,1),0,1):1;
    set(gradient,'day_strength',day);
    set(gradient,'sunset_factor',(time>85?-1:time<15?1:0)*(1-state.cloud_factor/100));
    set(gradient,'color_override',new THREE.Vector4(.5,.5,.5,1));
    set(gradient,'color_override_factor',clamp(remap(state.cloud_factor,40,100,0,1),0,1));
    nodes.get('StarsShader').visible=state.is_night;nodes.get('Moon').visible=state.is_night;
    if(state.is_night){
      const c=nodes.get('StarsShader').material.uniforms.background_color.value;
      // Match the surrounding backdrop to the original star-card background.
      scene.background.setRGB(c.x,c.y,c.z,THREE.SRGBColorSpace);
    }else scene.background.copy(originalBackground);
    set(nodes.get('Moon'),'phase',state.moon_phase);
    if(state.is_crescent!==undefined)set(nodes.get('Moon'),'is_crescent',Boolean(state.is_crescent));
    for(const [path,node] of nodes){
      if(path.includes('/PartiallyCloudy/'))set(node,'alpha_factor',factors.partial);
      if(path.includes('/VeryCloudy/'))set(node,'alpha_factor',factors.thick);
      if(path.includes('/PartiallyCloudy/')||path.includes('/VeryCloudy/'))set(node,'sunset_factor',factors.cloudSunset);
    }
    const sun=nodes.get('SunPathAnimation/SunHalo/SunObject');
    set(sun,'alpha_factor',state.is_night?0:Math.min(clamp(1.1-state.cloud_factor/100,0,1),1-4*(time/100-.5)**2));
    const q=4*(time/100-.5)**2;
    set(sun,'albedo',new THREE.Vector4(1,clamp(1.2-q*.7772,0,1),clamp(1.2-q*.93,0,1),1));
    const cover=nodes.get('SunPathAnimation/SunHalo/SunObject/SunCover');
    set(cover,'alpha_factor',state.is_night?0:Math.max(0,Math.min(clamp(state.cloud_factor/50,0,1),1-4*((time-25)/50-.5)**2)));
    // Original SunHaloAction track, sampled at the same normalized time.
    for(const [id,path] of [[6,'SunPathAnimation/SunHalo'],[7,'SunCenterAnimation/SunCenterEmpty']]){
      const data=block(sky,id),keys=nums(prop(data,'tracks/0/keys')),duration=Number(prop(data,'length'));
      const t=time/100*duration;let i=0;while(i+12<keys.length&&keys[i+12]<t)i+=12;
      const j=Math.min(i+12,keys.length-12),f=i===j?0:clamp((t-keys[i])/(keys[j]-keys[i]),0,1),node=nodes.get(path);
      node.matrixAutoUpdate=true;
      node.position.fromArray(keys,i+2).lerp(new THREE.Vector3().fromArray(keys,j+2),f);
      node.quaternion.fromArray(keys,i+5).slerp(new THREE.Quaternion().fromArray(keys,j+5),f);
      node.scale.fromArray(keys,i+9).lerp(new THREE.Vector3().fromArray(keys,j+9),f);
    }
    weatherRoot.updateMatrixWorld(true);
    const center=nodes.get('SunCenterAnimation/SunCenterEmpty');
    set(gradient,'center_position_x',clamp(remap(center.position.z,-3.937,3.973,.125,.8),.125,.8));
    // Godot clamp with reversed bounds in original script resolves to .673.
    set(gradient,'center_position_y',-.673);
    nodes.get('Snow').visible=factors.snow;nodes.get('Wind').visible=factors.wind>0;
    for(const e of emitters){
      e.mesh.visible=e.name==='Drops'||e.name.includes('Splash')?state.rain_factor>0:
        e.name==='Flakes'?factors.snow:e.name==='Wind'?factors.wind>0:factors.fog;
      e.mesh.count=e.name==='Drops'?factors.drops:e.name==='GroundSplash'?factors.groundSplash:
        e.name.includes('RoofSplash')?factors.roofSplash:e.name==='Wind'?factors.wind:e.max;
      if(e.name==='Wind')e.mesh.material.uniforms.alpha_factor.value=factors.windAlpha;
      if(e.name==='Fog')e.gravity[0]=factors.fogSpeed;
    }
    return {state,factors};
  }
  async function attachSolar(solar) {
    const text=await read('Energy/Load/Residential/Classic/SolarPanel.tscn');
    // glTF disambiguates the duplicated Godot root/mesh name: the mesh child
    // is SolarPanel2, not the root Spatial named SolarPanel.
    const panel=solar.getObjectByName('SolarPanel2');if(!panel)throw Error('Original SolarPanel mesh anchor missing');
    const frost=new THREE.Group();frost.name='OriginalFrost';panel.add(frost);
    for(const n of sceneNodes(text))if(n.header.includes('parent="SolarPanel/Frost"')){
      const mesh=new THREE.Mesh(plane,frostMaterial);mesh.name=n.header.match(/name="([^"]+)"/)[1];
      transform(mesh,n.properties);frost.add(mesh);
    }
    frost.userData.weatherFrost=true;frost.visible=factors.snow;return frost;
  }
  const dummy=new THREE.Object3D();
  function update(seconds){
    for(const m of materials)m.uniforms.currentTime.value=seconds;
    nodes.get('SkyGradient/CloudController/Lightning/LightningBolt').visible=factors.lightning;
    const elapsed=seconds-lastSwitch;
    // Original first-flash timing (.15/.12/.08/.18 after 1.5s).
    // The original controller starts one sequence when rain crosses >80;
    // it does not continuously retrigger for an unchanged weather payload.
    // Clicking the storm preset again supplies a fresh preview event.
    const t=elapsed-1.5;
    const bolt=t<0||t>.53?1:t<.15?1-t/.15:t<.27?(t-.15)/.12:t<.35?1-.8*(t-.27)/.08:.2+.8*(t-.35)/.18;
    set(nodes.get('SkyGradient/CloudController/Lightning/LightningBolt'),'lightning_factor',bolt);
    for(const e of emitters){
      if(!e.mesh.visible)continue;
      const attr=e.mesh.geometry.attributes.instanceAlpha;
      for(let i=0;i<e.mesh.count;i++){
        const seed=e.seeds[i],age=(seconds+seed[0]*e.life)%e.life,f=age/e.life;
        dummy.position.set((seed[1]*2-1)*e.extent[0],(seed[2]*2-1)*e.extent[1],(seed[3]*2-1)*e.extent[2]);
        dummy.position.x+=e.velocity*age+.5*e.gravity[0]*age*age;
        dummy.position.y+=.5*e.gravity[1]*age*age;dummy.position.z+=.5*e.gravity[2]*age*age;
        let scale=e.scale*(1-e.random*seed[4]);
        // Curves/ramps sampled in browser; CPUParticles RNG and spread differ.
        if(e.name.includes('Splash'))scale*=f<.446316?THREE.MathUtils.lerp(.0450451,.211712,f/.446316):THREE.MathUtils.lerp(.211712,0,(f-.446316)/.551579);
        dummy.scale.setScalar(scale);dummy.quaternion.identity();dummy.updateMatrix();e.mesh.setMatrixAt(i,dummy.matrix);
        attr.array[i]=(e.ramp?e.name==='Drops'?clamp(remap(f,.65445,.815126,0,.780392),0,.780392):Math.min(1,f/.095,(1-f)/.109):1)*e.colorAlpha;
      }
      attr.needsUpdate=true;e.mesh.instanceMatrix.needsUpdate=true;
    }
    scene.traverse(o=>{if(o.userData.weatherFrost)o.visible=factors.snow;});
  }
  apply(state);
  console.info('Original weather loaded',JSON.stringify({nodeCount:nodes.size,emitters:emitters.map(e=>({name:e.name,max:e.max}))}));
  return {apply,update,attachSolar};
}
