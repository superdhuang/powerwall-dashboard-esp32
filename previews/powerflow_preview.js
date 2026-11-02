import * as THREE from 'three';
import {powerflowPresets,scenarioRoute} from './continuous_powerflow.js';

function measureCableLength(mesh) {
  mesh.updateWorldMatrix(true,false);
  const uv=mesh.geometry.attributes.uv,position=mesh.geometry.attributes.position;
  const sections=new Map(),point=new THREE.Vector3();
  for(let i=0;i<uv.count;i++){
    const key=Math.round(uv.getX(i)*1e5);
    let section=sections.get(key);
    if(!section){section={u:uv.getX(i),x:0,y:0,z:0,count:0};sections.set(key,section);}
    point.fromBufferAttribute(position,i).applyMatrix4(mesh.matrixWorld);
    section.x+=point.x;section.y+=point.y;section.z+=point.z;section.count++;
  }
  const ordered=[...sections.values()].sort((a,b)=>a.u-b.u);
  let length=0;
  for(let i=1;i<ordered.length;i++){
    const a=ordered[i-1],b=ordered[i];
    length+=Math.hypot(b.x/b.count-a.x/a.count,b.y/b.count-a.y/a.count,b.z/b.count-a.z/a.count);
  }
  if(!(length>0))throw Error('Could not measure original wire length: '+mesh.name);
  return length;
}

export function setupPowerflowPreview(model) {
  const names={grid:'PowerflowGrid-1-A',load:'PowerflowLoad-1-A',battery:'PowerflowBatteryWithoutInverter-1-A'};
  const meshes={},ranges={},pathLengths={},offGridMarkers=[];
  model.traverse(node=>{
    if(node.name.toLowerCase().includes('offgridx')){
      offGridMarkers.push(node);
      node.visible=false;
    }
  });
  if(!offGridMarkers.length)throw Error('Original OffGridX marker missing');
  for(const [key,name] of Object.entries(names)){
    const mesh=model.getObjectByName(name);
    if(!mesh?.isMesh)throw Error('Original wire missing: '+name);
    meshes[key]=mesh;
    const uv=mesh.geometry.attributes.uv;
    let min=Infinity,max=-Infinity;
    for(let i=0;i<uv.count;i++){min=Math.min(min,uv.getX(i));max=Math.max(max,uv.getX(i));}
    ranges[key]=[min,max];
    pathLengths[key]=measureCableLength(mesh);
  }
  const group=document.querySelector('#powerflow-controls'),detail=document.querySelector('#powerflow-detail');
  const outageButton=document.querySelector('#grid-outage-toggle');
  const gridStatusDetail=document.querySelector('#grid-status-detail');
  outageButton.addEventListener('click',()=>{
    const offline=outageButton.getAttribute('aria-pressed')!=='true';
    for(const marker of offGridMarkers)marker.visible=offline;
    outageButton.setAttribute('aria-pressed',String(offline));
    outageButton.classList.toggle('active',offline);
    outageButton.textContent=offline?'恢復連網':'預覽斷網';
    gridStatusDetail.textContent=offline?'斷網預覽：地面紅色 X 顯示中':'市電正常：紅色 X 隱藏';
  });
  for(const preset of powerflowPresets){
    const button=document.createElement('button');button.type='button';button.textContent=preset.label;
    button.dataset.scenario=preset.id;button.setAttribute('aria-pressed','false');
    button.addEventListener('click',()=>{
      const route=scenarioRoute(ranges,preset.id,pathLengths),start=performance.now()/1000;
      for(const [key,mesh] of Object.entries(meshes)){
        const u=mesh.material.uniforms,branch=route.branches[key];
        mesh.visible=true;u.routeEnabled.value=!!branch;u.direction.value=0;
        if(!branch)continue;
        u.routeScale.value=branch.scale;u.routeOffset.value=branch.offset;
        u.routeTotal.value=route.total;u.routeTail.value=route.tail;
        u.routeGap.value=route.gap;u.routeSpeed.value=route.speed;u.routeStart.value=start;
      }
      for(const b of group.children){b.classList.toggle('active',b===button);b.setAttribute('aria-pressed',String(b===button));}
      group.dataset.current=preset.id;
      detail.textContent=preset.label+'｜沿原始線長等速流動；電流在 Gateway 接續'+(preset.sinks.length>1?'分流':'');
    });
    group.append(button);
  }
  [...group.children].find(b=>b.dataset.scenario==='Home-WiFi').click();
}
