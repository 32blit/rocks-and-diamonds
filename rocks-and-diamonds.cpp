#include <string>
#include <string.h>
#include <memory>
#include <cstdlib>

#include "rocks-and-diamonds.hpp"

using namespace blit;

#define PLAYER_TOP (player.position.y)
#define PLAYER_BOTTOM (player.position.y + player.size.y)
#define PLAYER_RIGHT (player.position.x + player.size.x)
#define PLAYER_LEFT (player.position.x)

constexpr uint16_t level_width = 64;
constexpr uint16_t level_height = 64;

const void* levels[] = {
  &asset_assets_level01_tmx,
  &asset_assets_level02_tmx
};

uint8_t *level_data;

Timer timer_level_update;
Timer timer_level_animate;
TileMap* level;

struct Feedback {
  bool rock_thunk;
};

struct Player {
  Point start;
  Point position;
  Point screen_location;
  uint32_t score;
  Vec2 camera;
  Point size = Point(1, 1);
  bool facing = true;
  bool has_key;
  uint32_t level;
  bool dead;
};

Player player;

Feedback feedback;

Mat3 camera;

enum entityType {
  // Terrain
  NOTHING = 0x00,
  DIRT = 0x01,
  WALL = 0x02,
  STAIRS = 0x03,
  LOCKED_STAIRS = 0x04,

  // Rocks and diamonds
  ROCK = 0x10,
  DIAMOND = 0x11,

  // Player animations... or lack thereof
  PLAYER = 0x30,
  PLAYER_FL = 0x31,
  PLAYER_SQUASHED = 0x3e,
  PLAYER_DEAD = 0x3f,

  // Collectable entities that aren't diamonds
  KEY_SILVER = 0x20,
  KEY_GOLD = 0x21,

  // Animations, ho boy this is ugly!
  DIRT_ANIM_1 = 0x50,
  DIRT_ANIM_2 = 0x51,
  DIRT_ANIM_3 = 0x52,
  DIRT_ANIM_4 = 0x53,

  // So ugly, oh boy!
  BOMB_ANIM_1 = 0x60,
  BOMB_ANIM_2 = 0x61,
  BOMB_ANIM_3 = 0x62,
  BOMB_ANIM_4 = 0x63,
  BOMB_ANIM_5 = 0x64,
  BOMB_ANIM_6 = 0x65,
};

// Line-interrupt callback for level->draw that applies our camera transformation
// This can be expanded to add effects like camera shake, wavy dream-like stuff, all the fun!
std::function<Mat3(uint8_t)> level_line_interrupt_callback = [](uint8_t y) -> Mat3 {
  (void)y; // Camera is updated elsewhere and is scanline-independent
  return camera;
};

void update_camera(uint32_t time) {
  (void)time;

  static uint32_t thunk_a_bunch = 0;
  // Create a camera transform that centers around the player's position
  if(player.camera.x < player.position.x) {
    player.camera.x += 0.1f;
  }
  if(player.camera.x > player.position.x) {
    player.camera.x -= 0.1f;
  }
  if(player.camera.y < player.position.y) {
    player.camera.y += 0.1f;
  }
  if(player.camera.y > player.position.y) {
    player.camera.y -= 0.1f;
  }

  if(feedback.rock_thunk) {
    thunk_a_bunch = 10;
    feedback.rock_thunk = 0;
  }

  camera = Mat3::identity();
  camera *= Mat3::translation(Vec2(player.camera.x * 8.0f, player.camera.y * 8.0f)); // offset to middle of world
  camera *= Mat3::translation(Vec2(-screen.bounds.w / 2, -screen.bounds.h / 2)); // transform to centre of framebuffer

  if(thunk_a_bunch){
    camera *= Mat3::translation(Vec2(
      float(blit::random() & 0x03) - 1.5f,
      float(blit::random() & 0x03) - 1.5f
    ));
    thunk_a_bunch--;
    vibration = thunk_a_bunch / 10.0f;
  }


}

Point level_first(entityType entity) {
  for(auto x = 0; x < level_width; x++) {
    for(auto y = 0; y < level_height; y++) {
      if (level_data[y * level_width + x] == entity) {
        return Point(x, y);
      } 
    }
  }
  return Point(-1, -1);
}

void level_set(Point location, entityType entity) {
  level_data[location.y * level_width + location.x] = entity;
}

bool player_at(Point location) {
  return (player.position.x == location.x && player.position.y == location.y);
}

entityType level_get(Point location) {
  if(location.y < 0 || location.x < 0 || location.y >= level_height || location.x >= level_width) {
    return WALL;
  }
  entityType entity = (entityType)level_data[location.y * level_width + location.x];
  if(entity == NOTHING && player_at(location)) {
    entity = PLAYER;
  }
  return entity;
}

void level_set(Point location, entityType entity, bool not_nothing) {
  if(not_nothing) {
    if(level_get(location) != NOTHING) {
      level_set(location, entity);
    }
  } else {
    level_set(location, entity);
  }
}

void animate_level(Timer &timer) {
  (void)timer;

  Point location = Point(0, 0);
  for(location.y = level_height - 1; location.y > -1; location.y--) {
    for(location.x = 0; location.x < level_width + 1; location.x++) {
      entityType current = level_get(location);

      if(current == DIRT_ANIM_4) {
        level_set(location, NOTHING);
      } else if(current == DIRT_ANIM_3) {
        level_set(location, DIRT_ANIM_4);
      } else if(current == DIRT_ANIM_2) {
        level_set(location, DIRT_ANIM_3);
      } else if(current == DIRT_ANIM_1) {
        level_set(location, DIRT_ANIM_2);
      }

      if(current == BOMB_ANIM_1) {
        level_set(location, BOMB_ANIM_2);
      } else if(current == BOMB_ANIM_2) {
        level_set(location, BOMB_ANIM_3);
      } else if(current == BOMB_ANIM_3) {
        level_set(location, BOMB_ANIM_4);
      } else if(current == BOMB_ANIM_4) {
        level_set(location, BOMB_ANIM_5);
      } else if(current == BOMB_ANIM_5) {
        level_set(location, BOMB_ANIM_6);
      } else if(current == BOMB_ANIM_6) {
        level_set(location, NOTHING);
        level_set(location + Point(0, 1), DIRT_ANIM_1, true);
        level_set(location + Point(0, -1), DIRT_ANIM_1, true);
        level_set(location + Point(1, 0), DIRT_ANIM_1, true);
        level_set(location + Point(-1, 0), DIRT_ANIM_1, true);
        level_set(location + Point(1, 1), DIRT_ANIM_1, true);
        level_set(location + Point(-1, -1), DIRT_ANIM_1, true);
        level_set(location + Point(-1, 1), DIRT_ANIM_1, true);
        level_set(location + Point(1, -1), DIRT_ANIM_1, true);
      }
    }
  }
}

void update_level(Timer &timer) {
  (void)timer;

  Point location = Point(0, 0);
  for(location.y = level_height - 1; location.y > 0; location.y--) {
    for(location.x = 0; location.x < level_width; location.x++) {
      Point location_below = location + Point(0, 1);
      entityType current = level_get(location);
      entityType below = level_get(location_below);

      for(entityType check_entity : {ROCK, DIAMOND}) {
        if(current == check_entity) {
          if (below == NOTHING) {
            // If the space underneath is empty, fall down
            level_set(location, NOTHING);
            level_set(location_below, check_entity);

            if(check_entity == ROCK) {
              // Add a little *THUNK* effect for rocks falling directly down
              Point location_land = location_below + Point(0, 1);
              switch (level_get(location_land)) {
                case WALL:
                  feedback.rock_thunk = true;
                  break;
                case PLAYER:
                  player.dead = true;
                  feedback.rock_thunk = true;
                  level_set(location_land, PLAYER_SQUASHED);
                default:
                  break;
              }
            }
          } else if (below == PLAYER_SQUASHED && check_entity == ROCK) {
            level_set(location, NOTHING);
            level_set(location_below, PLAYER_DEAD);
          } else if (below == ROCK || below == DIAMOND) {
            // If the space below is a rock or a diamond, check to the left/right
            // and "roll" down the stack
            entityType left = level_get(location + Point(-1, 0));
            entityType below_left = level_get(location + Point(-1, 1));
            entityType right = level_get(location + Point(1, 0));
            entityType below_right = level_get(location + Point(1, 1));

            if(left == NOTHING && below_left == NOTHING){
              level_set(location, NOTHING);
              level_set(location + Point(-1, 1), check_entity);
            } else if(right == NOTHING && below_right == NOTHING){
              level_set(location, NOTHING);
              level_set(location + Point(1, 1), check_entity);
            }
          }
        }
      }

    }
  }
}

void new_game(uint32_t level) {
  // Get a pointer to the map header
  TMX *tmx = (TMX *)levels[level];

  // Bail if the map is oversized
  if(tmx->width > level_width) return;
  if(tmx->height > level_height) return;

  // Clear the level data
  memset(level_data, 0, level_width * level_height);

  // Load the level data from the map memory
  for(auto x = 0u; x < tmx->width; x++) {
    for(auto y = 0u; y < tmx->height; y++) {
      auto src = y * tmx->width + x;
      auto dst = y * level_width + x;
      level_data[dst] = tmx->data[src];
    }
  }

  player.start = level_first(PLAYER);
  level_set(player.start, NOTHING);
  player.position = player.start;
  player.camera = Vec2(player.position.x, player.position.y);
  player.has_key = false;
  player.score = 0;
  player.dead = false;

  player.screen_location = Point(screen.bounds.w / 2, screen.bounds.h / 2);
  player.screen_location += Point(1, 1);
}

void init() {
  set_screen_mode(ScreenMode::lores);

  // Load the spritesheet from the linked binary blob
  screen.sprites = Surface::load(asset_sprites);

  // Allocate memory for the writeable copy of the level
  level_data = (uint8_t *)malloc(level_width * level_height);

  // Load our level data into the TileMap
  level = new TileMap((uint8_t *)level_data, nullptr, Size(level_width, level_height), screen.sprites);
  
  timer_level_update.init(update_level, 250, -1);
  timer_level_update.start();
  
  timer_level_animate.init(animate_level, 100, -1);
  timer_level_animate.start();

  new_game(0);
}

void render(uint32_t time) {
  (void)time;

  screen.pen = Pen(0, 0, 0);
  screen.clear();

  // Draw our level
  level->draw(&screen, Rect(0, 0, screen.bounds.w, screen.bounds.h), level_line_interrupt_callback);

  // Draw our character sprite
  if(!player.dead) {
    screen.sprite(player.facing ? entityType::PLAYER : entityType::PLAYER_FL, player.screen_location);
  }

  // Draw the header bar
  screen.pen = Pen(255, 255, 255);
  screen.rectangle(Rect(0, 0, screen.bounds.w, 10));
  screen.pen = Pen(0, 0, 0);
  screen.text("Level: " + std::to_string(player.level) + " Score: " + std::to_string(player.score), minimal_font, Point(2, 2));

  if(player.has_key) {
    screen.sprite(entityType::KEY_SILVER, Point(screen.bounds.w - 10, 1));
  }
  // screen.text(std::to_string(player.position.x), minimal_font, Point(0, 0));
  // screen.text(std::to_string(player.position.y), minimal_font, Point(0, 10));
}

void update(uint32_t time) {
  (void)time;

  Point movement = Point(0, 0);

  if(buttons.pressed & Button::B) {
    new_game(player.level);
  }

  if(buttons.pressed & Button::A) {
    if(level_get(player.position + Point(0, 1)) == NOTHING) {
      level_set(player.position + Point(0, 1), BOMB_ANIM_1);
    }
  }

  if(!player.dead) {
    if(buttons.pressed & Button::DPAD_UP) {
      movement.y = -1;
    }
    if(buttons.pressed & Button::DPAD_DOWN) {
      movement.y = 1;
    }
    if(buttons.pressed & Button::DPAD_LEFT) {
      player.facing = false;
      movement.x = -1;
    }
    if(buttons.pressed & Button::DPAD_RIGHT) {
      player.facing = true;
      movement.x = 1;
    }

    player.position += movement;

    entityType standing_on = level_get(player.position);

    switch(standing_on) {
      case WALL:
        player.position -= movement;
        break;
      case ROCK:
        // Push rocks if there's an empty space the other side of them
        if(movement.x > 0 && level_get(player.position + Point(1, 0)) == NOTHING){
          level_set(player.position + Point(1, 0), ROCK);
          level_set(player.position, NOTHING);
        }
        else if(movement.x < 0 && level_get(player.position + Point(-1, 0)) == NOTHING){
          level_set(player.position + Point(-1, 0), ROCK);
          level_set(player.position, NOTHING);
        }
        else {
          player.position -= movement;
        }
        break;
      case DIAMOND:
        // Collect diamonds!
        player.score += 1;
        level_set(player.position, NOTHING);
        break;
      case DIRT:
        // Dig dirt!
        level_set(player.position, DIRT_ANIM_1);
        break;
      case KEY_SILVER:
        player.has_key = true;
        level_set(player.position, NOTHING);
        break;
      case LOCKED_STAIRS:
        if(player.has_key) {
          level_set(player.position, STAIRS);
        }
        player.position -= movement;
        break;
      case STAIRS:
        player.level++;
        new_game(player.level);
        break;
      default:
        break;
    }
  }

  update_camera(time);
}