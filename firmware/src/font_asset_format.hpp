#pragma once
#include <stdint.h>
#pragma pack(push, 1)
struct FontAssetHeader { char magic[8]; uint32_t version, bytes, count, checksum; };
struct FontAssetRecord { char name[40]; uint16_t height; int16_t baseline, underline; uint16_t thickness; int32_t fallback; uint32_t glyphOffset, glyphCount, kernOffset, kernCount; };
struct FontAssetGlyph { uint32_t codepoint, bitmapOffset, bitmapBytes; uint16_t advance, width, height; int16_t x, y; uint8_t bpp, reserved; };
struct FontAssetKern { uint32_t pair; int16_t adjust, reserved; };
#pragma pack(pop)
static_assert(sizeof(FontAssetHeader)==24 && sizeof(FontAssetRecord)==68 && sizeof(FontAssetGlyph)==24 && sizeof(FontAssetKern)==8, "Font asset ABI");
inline uint32_t fontAssetHash(const uint8_t* p, size_t count) { uint32_t h=2166136261U; while(count--) h=(h^*p++)*16777619U; return h; }
