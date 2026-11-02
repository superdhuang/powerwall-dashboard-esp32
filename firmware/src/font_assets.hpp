#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font_asset_format.hpp"
#if defined(POWERWALL_NATIVE480)
#include "native480_font_names.hpp"
#else
#include "font_asset_names.hpp"
#endif
#if defined(POWERWALL_NATIVE480)
inline constexpr const char* const* powerwallFontNames = kNative480FontNames;
inline constexpr uint32_t powerwallFontNameCount = sizeof(kNative480FontNames) / sizeof(kNative480FontNames[0]);
#else
#define PW_FONT_NAME(i,n) #n,
inline const char* const powerwallFontNames[]={POWERWALL_FONT_LIST(PW_FONT_NAME)};
#undef PW_FONT_NAME
inline constexpr uint32_t powerwallFontNameCount = sizeof(powerwallFontNames) / sizeof(powerwallFontNames[0]);
#endif
#if defined(ESP32)
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#else
#include <fstream>
#endif
namespace powerwall_fonts {
inline uint8_t* data=nullptr;
inline size_t bytes=0;
inline const FontAssetRecord* records=nullptr;
inline lv_font_t fonts[64]{};
inline uint32_t count=0;
inline bool range(uint32_t offset,uint32_t n){return offset<=bytes && n<=bytes-offset;}
inline const FontAssetGlyph* findGlyph(const FontAssetRecord& r,uint32_t cp){
 const auto* list=reinterpret_cast<const FontAssetGlyph*>(data+r.glyphOffset);size_t lo=0,hi=r.glyphCount;
 while(lo<hi){size_t m=lo+(hi-lo)/2;if(list[m].codepoint==cp)return list+m;if(list[m].codepoint<cp)lo=m+1;else hi=m;}return nullptr;
}
inline bool descriptor(const lv_font_t* font,lv_font_glyph_dsc_t* out,uint32_t cp,uint32_t next){
 const auto& r=*static_cast<const FontAssetRecord*>(font->dsc);const auto* g=findGlyph(r,cp);if(!g)return false;
 int advance=g->advance;
 if(cp<=0xffff && next<=0xffff){uint32_t key=(cp<<16)|next;const auto* k=reinterpret_cast<const FontAssetKern*>(data+r.kernOffset);size_t lo=0,hi=r.kernCount;while(lo<hi){size_t m=lo+(hi-lo)/2;if(k[m].pair==key){advance+=k[m].adjust;break;}if(k[m].pair<key)lo=m+1;else hi=m;}}
 out->resolved_font=font;out->adv_w=advance;out->box_w=g->width;out->box_h=g->height;out->ofs_x=g->x;out->ofs_y=g->y;out->bpp=g->bpp;out->is_placeholder=0;return true;
}
inline const uint8_t* bitmap(const lv_font_t* font,uint32_t cp){const auto* g=findGlyph(*static_cast<const FontAssetRecord*>(font->dsc),cp);return g?data+g->bitmapOffset:nullptr;}
inline bool validate(){
 if(bytes<sizeof(FontAssetHeader))return false;FontAssetHeader h;memcpy(&h,data,sizeof(h));
 if(memcmp(h.magic,"PWFONT1",8)||h.version!=1||h.bytes!=bytes||h.count!=powerwallFontNameCount||h.count>64||!range(sizeof(h),h.count*sizeof(FontAssetRecord))||fontAssetHash(data+sizeof(h),bytes-sizeof(h))!=h.checksum)return false;
 records=reinterpret_cast<const FontAssetRecord*>(data+sizeof(h));
 for(uint32_t i=0;i<h.count;++i){const auto& r=records[i];if(!memchr(r.name,0,sizeof(r.name))||strcmp(r.name,powerwallFontNames[i])||r.height==0||r.fallback< -1||r.fallback>=int(h.count)||r.glyphCount>65536||r.kernCount>65536||!range(r.glyphOffset,r.glyphCount*sizeof(FontAssetGlyph))||!range(r.kernOffset,r.kernCount*sizeof(FontAssetKern)))return false;
 const auto* g=reinterpret_cast<const FontAssetGlyph*>(data+r.glyphOffset);for(uint32_t j=0;j<r.glyphCount;++j)if((j && g[j-1].codepoint>=g[j].codepoint)||g[j].bpp!=4||g[j].bitmapBytes!=(uint64_t(g[j].width)*g[j].height*4+7)/8||!range(g[j].bitmapOffset,g[j].bitmapBytes))return false;
 const auto* k=reinterpret_cast<const FontAssetKern*>(data+r.kernOffset);for(uint32_t j=1;j<r.kernCount;++j)if(k[j-1].pair>=k[j].pair)return false;
 int f=i;for(uint32_t depth=0;f>=0;++depth){if(depth>=h.count)return false;f=records[f].fallback;if(f>=int(h.count)||f< -1)return false;}
 }
 for(uint32_t i=0;i<h.count;++i){fonts[i]={};fonts[i].get_glyph_dsc=descriptor;fonts[i].get_glyph_bitmap=bitmap;fonts[i].line_height=records[i].height;fonts[i].base_line=records[i].baseline;fonts[i].underline_position=records[i].underline;fonts[i].underline_thickness=records[i].thickness;fonts[i].dsc=records+i;fonts[i].fallback=records[i].fallback<0?nullptr:fonts+records[i].fallback;}
 count=h.count;return true;
}
inline bool load(){
 if(count)return true;
#if defined(ESP32)
 static fs::SPIFFSFS assets;
 if(!assets.begin(false,"/assets",2,"assets")){printf("[Fonts] assets mount failed; preserved flash\n");return false;}
 auto f=assets.open("/fonts.pwf",FILE_READ);if(!f)return false;bytes=f.size();if(bytes>4*1024*1024||bytes<sizeof(FontAssetHeader))return false;
 data=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data)return false;size_t got=f.read(data,bytes);f.close();if(got!=bytes){free(data);data=nullptr;return false;}
#else
 const char* path=getenv("POWERWALL_FONT_ASSETS");
 if(!path){
#if defined(POWERWALL_NATIVE480)
  path="firmware/assets/native480/fonts.pwf";
#else
  path="firmware/assets/fonts.pwf";
#endif
 }
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return false;bytes=static_cast<size_t>(f.tellg());if(bytes>4*1024*1024||bytes<sizeof(FontAssetHeader))return false;f.seekg(0);data=static_cast<uint8_t*>(malloc(bytes));if(!data||!f.read(reinterpret_cast<char*>(data),bytes)){free(data);data=nullptr;return false;}
#endif
 if(!validate()){free(data);data=nullptr;records=nullptr;bytes=0;printf("[Fonts] invalid asset pack; UI creation refused\n");return false;}
 printf("[Fonts] loaded %u fonts, %u bytes into runtime memory\n",unsigned(count),unsigned(bytes));return true;
}
}
inline const lv_font_t* powerwallFont(unsigned index){return index<powerwall_fonts::count?powerwall_fonts::fonts+index:nullptr;}
inline const lv_font_t* powerwallFontByName(const char* name){
 if(!name)return nullptr;
 for(uint32_t i=0;i<powerwall_fonts::count;++i)if(strcmp(powerwall_fonts::records[i].name,name)==0)return powerwall_fonts::fonts+i;
 return nullptr;
}
inline bool loadPowerwallFontAssets(){return powerwall_fonts::load();}
