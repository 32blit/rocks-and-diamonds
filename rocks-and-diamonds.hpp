#include <stdint.h>

#include "32blit.hpp"
#include "assets.hpp"

void init();
void update(uint32_t time);
void render(uint32_t time);

void update_camera(uint32_t time);

void update_level(blit::Timer &timer);
void animate_level(blit::Timer &timer);