#include "cjk_fonts.hpp"
#include "font_assets.hpp"
#include "font_asset_names.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#define PTR(i,n) &n,
static const lv_font_t* sourceFonts[]={POWERWALL_FONT_LIST(PTR)};
static bool same(const lv_font_glyph_dsc_t& a,const lv_font_glyph_dsc_t& b){return a.adv_w==b.adv_w&&a.box_w==b.box_w&&a.box_h==b.box_h&&a.ofs_x==b.ofs_x&&a.ofs_y==b.ofs_y&&a.bpp==b.bpp;}
int main(){
 if(!loadPowerwallFontAssets())return 1;
 size_t glyphs=0,pairs=0;
 for(unsigned i=0;i<powerwall_fonts::count;++i){const auto* a=sourceFonts[i];const auto* b=powerwallFont(i);if(a->line_height!=b->line_height||a->base_line!=b->base_line)return 2;
 for(uint32_t cp=9;cp<=0xffff;++cp){lv_font_glyph_dsc_t ad{},bd{};bool av=a->get_glyph_dsc(a,&ad,cp,0),bv=b->get_glyph_dsc(b,&bd,cp,0);if(av!=bv||(av&&!same(ad,bd))){printf("metric mismatch font%u U+%x\n",i,cp);return 3;}if(!av)continue;
 size_t bytes=(ad.box_w*ad.box_h*ad.bpp+7)/8;if(bytes&&memcmp(a->get_glyph_bitmap(a,cp),b->get_glyph_bitmap(b,cp),bytes)){printf("bitmap mismatch font%u U+%x\n",i,cp);return 4;}++glyphs;
 if(cp>=32&&cp<=126)for(uint32_t next=32;next<=126;++next){a->get_glyph_dsc(a,&ad,cp,next);b->get_glyph_dsc(b,&bd,cp,next);if(!same(ad,bd)){printf("kerning mismatch font%u %x/%x\n",i,cp,next);return 5;}++pairs;}
 }}
 // Reject corrupt/truncated packs, an ABI name mismatch, and fallback cycles.
 using namespace powerwall_fonts;uint8_t* good=data;size_t goodBytes=bytes;std::vector<uint8_t> bad(good,good+goodBytes);
 data=bad.data();bad.back()^=1;if(validate())return 6;bad.back()^=1;bytes=10;if(validate())return 7;bytes=goodBytes;
 auto* rec=reinterpret_cast<FontAssetRecord*>(bad.data()+sizeof(FontAssetHeader));auto* h=reinterpret_cast<FontAssetHeader*>(bad.data());rec[0].fallback=0;h->checksum=fontAssetHash(bad.data()+sizeof(*h),bad.size()-sizeof(*h));if(validate())return 8;
 data=good;bytes=goodBytes;if(!validate())return 9;
 printf("[FontAssets] PASS: %u fonts, %zu glyphs byte-identical, %zu kerning pairs; corrupt/truncated/cyclic packs rejected\n",count,glyphs,pairs);return 0;
}
