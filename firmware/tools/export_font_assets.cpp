#include "cjk_fonts.hpp"
#include "font_asset_format.hpp"
#include "font_asset_names.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#define PTR(i,n) &n,
static const lv_font_t* fonts[]={POWERWALL_FONT_LIST(PTR)};
#define NAME(i,n) #n,
static const char* names[]={POWERWALL_FONT_LIST(NAME)};
static std::vector<uint8_t> output;
template<class T> uint32_t append(const std::vector<T>& v) { uint32_t pos=output.size(); const auto* p=reinterpret_cast<const uint8_t*>(v.data()); output.insert(output.end(),p,p+v.size()*sizeof(T)); return pos; }
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 const size_t count=sizeof(fonts)/sizeof(fonts[0]);
 output.resize(sizeof(FontAssetHeader)+count*sizeof(FontAssetRecord));
 std::vector<FontAssetRecord> records(count);
 for(size_t i=0;i<count;++i){
  auto* font=fonts[i]; auto& rec=records[i]; std::snprintf(rec.name,sizeof(rec.name),"%s",names[i]);
  rec.height=font->line_height;rec.baseline=font->base_line;rec.underline=font->underline_position;rec.thickness=font->underline_thickness;rec.fallback=-1;
  for(size_t j=0;j<count;++j)if(font->fallback==fonts[j])rec.fallback=j;
  if(font->fallback && rec.fallback<0)throw std::runtime_error("unlisted fallback");
  std::vector<FontAssetGlyph> glyphs;std::vector<FontAssetKern> kern;
  for(uint32_t cp=9;cp<=0xffff;++cp){
   if(cp<32 && cp!=9)continue;
   lv_font_glyph_dsc_t d{}; if(!font->get_glyph_dsc(font,&d,cp,0) || d.is_placeholder)continue;
   if(d.bpp!=4)throw std::runtime_error("unexpected font bpp");
   FontAssetGlyph g{cp,0,uint32_t((d.box_w*d.box_h*d.bpp+7)/8),d.adv_w,d.box_w,d.box_h,d.ofs_x,d.ofs_y,d.bpp,0};
   g.bitmapOffset=output.size(); const auto* bitmap=font->get_glyph_bitmap(font,cp);
   if(g.bitmapBytes){if(!bitmap)throw std::runtime_error("missing bitmap");output.insert(output.end(),bitmap,bitmap+g.bitmapBytes);}
   glyphs.push_back(g);
   if(cp>=32 && cp<=126)for(uint32_t next=32;next<=126;++next){lv_font_glyph_dsc_t k{};if(font->get_glyph_dsc(font,&k,cp,next) && k.adv_w!=d.adv_w)kern.push_back({(cp<<16)|next,int16_t(int(k.adv_w)-int(d.adv_w)),0});}
  }
  rec.glyphCount=glyphs.size();rec.glyphOffset=append(glyphs);rec.kernCount=kern.size();rec.kernOffset=append(kern);
  std::printf("%s: %u glyphs, %u kerning pairs\n",rec.name,rec.glyphCount,rec.kernCount);
 }
 std::memcpy(output.data()+sizeof(FontAssetHeader),records.data(),records.size()*sizeof(FontAssetRecord));
 FontAssetHeader header{{'P','W','F','O','N','T','1',0},1,uint32_t(output.size()),uint32_t(count),fontAssetHash(output.data()+sizeof(FontAssetHeader),output.size()-sizeof(FontAssetHeader))};
 std::memcpy(output.data(),&header,sizeof(header));FILE* f=std::fopen(argv[1],"wb");if(!f)return 3;bool ok=std::fwrite(output.data(),1,output.size(),f)==output.size();std::fclose(f);std::printf("Font pack: %zu bytes\n",output.size());return ok?0:4;
}
