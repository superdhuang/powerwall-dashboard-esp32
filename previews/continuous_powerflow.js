// Build one cumulative route using the original GLB's wire UV coordinates.
// Grid U decreases toward Gateway; Load U increases away from Gateway.
export function continuousRoute(gridMin,gridMax,loadMin,loadMax) {
  const gridLength=gridMax-gridMin,loadLength=loadMax-loadMin;
  if(!(gridLength>0&&loadLength>0))throw Error('Invalid original wire UV range');
  return {grid:{scale:-1,offset:gridMax},load:{scale:1,offset:gridLength-loadMin},
    gridLength,total:gridLength+loadLength,tail:9*.6,gap:1,speed:9/2};
}
export const powerflowPresets=[
  {id:'grid_battery',label:'電網 → Powerwall',source:'grid',sinks:['battery']},
  {id:'Home-WiFi',label:'Powerwall → 住家',source:'battery',sinks:['load']},
  {id:'Home-WiFi',label:'電網 → 住家',source:'grid',sinks:['load']},
  {id:'grid_both',label:'電網 → Powerwall＋住家',source:'grid',sinks:['battery','load']}
];
export function scenarioRoute(ranges,id,pathLengths={}) {
  const preset=powerflowPresets.find(p=>p.id===id);
  if(!preset)throw Error('Unknown powerflow scenario '+id);
  const uvLength=name=>ranges[name][1]-ranges[name][0];
  const length=name=>pathLengths[name]??uvLength(name);
  const scale=name=>length(name)/uvLength(name);
  const sourceLength=length(preset.source),sourceScale=scale(preset.source);
  const branches={[preset.source]:{scale:-sourceScale,offset:ranges[preset.source][1]*sourceScale}};
  for(const name of preset.sinks){
    const sinkScale=scale(name);
    branches[name]={scale:sinkScale,offset:sourceLength-ranges[name][0]*sinkScale};
  }
  // Keep physical velocity equal to the previous Powerwall animation, while
  // mapping each original wire's UV coordinates to its measured world length.
  const referenceScale=scale('battery');
  return {preset,branches,sourceLength,total:sourceLength+Math.max(...preset.sinks.map(length)),
    tail:9*.6*referenceScale,gap:1*referenceScale,speed:9/2*referenceScale};
}
export function routeHead(seconds,route) {
  return (seconds*route.speed)%(route.total+route.tail+route.gap);
}
export function routePulse(distance,head,tail) {
  const age=head-distance;
  return age>=0&&age<=tail?(1-age/tail)**2:0;
}
