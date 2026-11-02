// Representative inputs, NOT a Tesla weather-type enumeration.
export const weatherPresets = [
  {id:'clear', label:'晴天', cloud_factor:0},
  {id:'partly', label:'多雲', cloud_factor:40},
  {id:'overcast', label:'陰天', cloud_factor:100},
  {id:'rain', label:'下雨', cloud_factor:85, rain_factor:50},
  {id:'storm', label:'強雨／閃電', cloud_factor:100, rain_factor:100},
  {id:'snow', label:'下雪／結霜', cloud_factor:85, snow_factor:100},
  {id:'wind', label:'強風', cloud_factor:60, wind_speed:85},
  {id:'sunrise', label:'日出', cloud_factor:10, time_of_day:12},
  {id:'sunset', label:'日落', cloud_factor:10, time_of_day:88},
  {id:'night', label:'夜晚／月亮', cloud_factor:0, time_of_day:0, is_night:true},
];
export function weatherInput(preset) {
  return {cloud_factor:0,rain_factor:0,snow_factor:0,wind_speed:0,
    time_of_day:50,is_night:false,moon_phase:0.39,...preset};
}
const clamp = (v,a=0,b=1) => Math.max(a,Math.min(b,v));
export const remap = (v,a,b,c,d) => c+(v-a)*(d-c)/(b-a);
// Direct translations from SunCover.gd / Wind.gd / EnergyRain.gd.
export function weatherFactors(w) {
  const t=w.is_night?0:w.time_of_day;
  const opacity=t<15?clamp(remap(t,0,15,.3,1),.3,1):t>85?clamp(remap(t,85,100,1,.3),.3,1):1;
  const sunset=w.time_of_day>85?clamp(remap(w.time_of_day,85,90,0,1)):
    w.time_of_day<15?clamp(remap(w.time_of_day,10,15,1,0)):0;
  return {
    partial:clamp(remap(w.cloud_factor,0,60,.1,1))*opacity,
    thick:(w.cloud_factor>50?clamp(remap(w.cloud_factor,50,100,.6,1)):0)*opacity,
    fog:w.cloud_factor>50,snow:w.snow_factor>1,lightning:w.rain_factor>80,
    cloudSunset:w.is_night?0:Math.min(1,sunset*(1-w.cloud_factor/100)*1.5),
    drops:Math.floor(w.rain_factor*5),groundSplash:Math.ceil(w.rain_factor*.5),roofSplash:Math.ceil(w.rain_factor*.3),
    wind:Math.floor(clamp(remap(w.wind_speed,50,85,15,33),0,33)),
    windAlpha:clamp(remap(w.wind_speed,60,85,.5,1)),
    fogSpeed:clamp(remap(w.wind_speed,50,85,-.026,-.033),-.4,0)
  };
}
