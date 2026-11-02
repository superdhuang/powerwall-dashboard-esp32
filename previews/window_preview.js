import * as THREE from 'three';
import {numericValues} from './godot_resource_values.js';

// User requests only day/night, not simulated power failures. Colors come
// from Classic/HomeAlbedo.gd sunny_window_albedo / dark_window_albedo.
export const windowPresets = [
  {label:'白天',weather:'晴天',night:false,color:[.45,.46,.49]},
  {label:'晚上',weather:'夜晚／月亮',night:true,color:[1,.98,.93]}
];

export async function loadOriginalWindowMaterial() {
  const root='../recovered/gdre_project/';
  const path='Energy/Load/Residential/Classic/Window.tres';
  const response=await fetch(root+path);
  if(!response.ok)throw Error('Cannot load '+path);
  const source=await response.text();
  const field=name=>source.match(new RegExp('^shader_param/'+name+' = (.+)$','m'))?.[1];
  const glow=numericValues(field('glow_color'));
  const texturePath=source.match(/path="res:\/\/([^"]+)" type="Texture"/)?.[1];
  const texture=await new THREE.TextureLoader().loadAsync(root+texturePath);
  texture.flipY=false;texture.colorSpace=THREE.NoColorSpace;
  const material=new THREE.ShaderMaterial({
    name:'OriginalClassicWindow',
    uniforms:{texture_window:{value:texture},glow_color:{value:new THREE.Vector3(...glow.slice(0,3))},
      brightness:{value:Number(field('brightness'))},alpha:{value:Number(field('alpha'))}},
    vertexShader:`varying vec2 vUv;
      void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}`,
    // Exact unshaded Window.shader formula. Original GLES2 outputs display
    // space directly; no added reflection, bloom, light or emissive multiplier.
    fragmentShader:`uniform sampler2D texture_window;uniform vec3 glow_color;
      uniform float brightness;uniform float alpha;varying vec2 vUv;
      void main(){vec3 data=texture2D(texture_window,vUv).rgb;
        gl_FragColor=vec4(glow_color*data.r*brightness,alpha);}`
  });
  return material;
}

export function setupWindowPreview(material,weatherReady) {
  const group=document.querySelector('#window-controls');
  const detail=document.querySelector('#window-detail');
  function apply(isNight){
    const preset=windowPresets.find(p=>p.night===isNight);
    material.uniforms.brightness.value=1;
    material.uniforms.glow_color.value.fromArray(preset.color);
    for(const b of group.children){
      const selected=b.textContent===preset.label;
      b.classList.toggle('active',selected);b.setAttribute('aria-pressed',String(selected));
    }
    detail.textContent=isNight?'晚上：原始暖白窗戶':'白天：原始灰藍窗戶';
  }
  for(const preset of windowPresets){
    const button=document.createElement('button');button.type='button';button.textContent=preset.label;
    button.setAttribute('aria-pressed','false');
    button.addEventListener('click',async()=>{
      apply(preset.night);
      await weatherReady;
      [...document.querySelectorAll('#weather-controls button')].find(b=>b.textContent===preset.weather)?.click();
    });
    group.append(button);
  }
  // Existing weather controls must not leave the windows in the wrong state.
  const weatherGroup=document.querySelector('#weather-controls');
  const sync=()=>apply(weatherGroup.dataset.current==='night');
  weatherGroup.addEventListener('click',event=>{if(event.target.closest('button'))sync();});
  apply(false);
  weatherReady.then(sync);
}
