#ifndef TGA_H_
#define TGA_H_

#include <stdint.h>


#define TGA_NO_COLOR_MAP  0
#define TGA_UNCOMPRESSED_TRUE_COLOR  2
#define TGA_UNCOMPRESSED_GRAYSCALE   3
#define TGA_ORIGIN_LOWER_LEFT 0
#define TGA_ORIGIN_UPPER_LEFT 1


struct tga_header {
  uint8_t   id_len;
  uint8_t   map_type;
  uint8_t   img_type;
  uint16_t  map_idx  __packed;
  uint16_t  map_len  __packed;
  uint8_t   map_elemsz;
  uint16_t  img_x;
  uint16_t  img_y;
  uint16_t  img_w;
  uint16_t  img_h;
  uint8_t   img_depth;
  uint8_t   img_alpha   : 4;
  uint8_t   _reserved   : 1;
  uint8_t   img_origin  : 1;
  uint8_t   img_store   : 2;
};

_Static_assert(sizeof(struct tga_header) == 18, "struct tga_header not packed");


#endif /* TGA_H_ */

