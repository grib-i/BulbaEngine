#include "text_2d.h"

static void animate(Demo_Context *demo) {
  if (demo->scene->text2d_count > 3) {
    BLB_Text2D *text = demo->scene->text2d[3];

    text->rotation = 8 * sinf((float)demo->elapsed);
    char value[80];

    snprintf(value, sizeof(value), "Elapsed: %.2f seconds", demo->elapsed);
    BLB_SetText2D(text, value);
  }
}

int Demo_Text2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "Text2D: size, color and animated content", settings))
    return Demo_Finish(&demo, NULL);

  for (int i = 0; i < (int)Demo_Count(&demo, 5); ++i) {
    BLB_Text2D *text =
        Demo_Text(&demo, "BulbaEngine - Text2D", 180, 180 + i * 450.0f / Demo_Count(&demo, 5), 20 + i % 5 * 8.0f);

    if (!text)
      break;

    text->color.r = (unsigned char)(255 + i * 35);
    text->color.g = 220;
  }
  return Demo_Finish(&demo, animate);
}
