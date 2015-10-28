#ifndef _SWAY_FOCUS_H
#define _SWAY_FOCUS_H
enum movement_direction {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_PARENT
};

#include "container.h"

// focused_container - the container found by following the `focused` pointer
// from a given container to a container with `is_focused` boolean set
// ---
// focused_view - the container found by following the `focused` pointer from a
// given container to a view.
// ---

swayc_t *get_focused_container(swayc_t *parent);
swayc_t *get_focused_view(swayc_t *parent);
swayc_t *get_focused_float(swayc_t *ws);

enum movement_direction get_movement_direction(char* direction);

bool set_focused_container(swayc_t *container);
bool set_focused_container_for(swayc_t *ancestor, swayc_t *container);

// lock focused container/view. locked by windows with OVERRIDE attribute
// and unlocked when they are destroyed

extern bool locked_container_focus;
extern bool locked_view_focus;


bool move_focus(enum movement_direction direction);

#endif

