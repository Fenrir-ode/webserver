#include "../log.h"

int get_image_region(char *r)
{
  const unsigned char region_code[] = {'J', 'T', 'U', 'B', 'K', 'A', 'E', 'L'};
  const char *region_str[] = {
      "Japan",
      "Taiwan",
      "USA",
      "Brazil",
      "Korea",
      "Asia Pal",
      "Europe",
      "Latin America"};
  for (int i = 0; i < sizeof(region_code); i++)
  {
    if ((r[0] | 0x40) == (region_code[i] | 0x40))
    {
      log_info("Patch region to %s", region_str[i]);
      return i;
    }
  }
  log_error("unknown region\n");
  return -1;
}

