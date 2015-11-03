#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <wlc/wlc.h>
#include <ctype.h>

#include "handlers.h"
#include "log.h"
#include "layout.h"
#include "config.h"
#include "commands.h"
#include "stringop.h"
#include "workspace.h"
#include "container.h"
#include "output.h"
#include "focus.h"
#include "input_state.h"
#include "resize.h"

// Event should be sent to client
#define EVENT_PASSTHROUGH false

// Event handled by sway and should not be sent to client
#define EVENT_HANDLED true

/* Handles */

static bool handle_output_created(wlc_handle output) {
	swayc_t *op = new_output(output);

	// Visibility mask to be able to make view invisible
	wlc_output_set_mask(output, VISIBLE);

	if (!op) {
		return false;
	}

	refresh_view(op);

	// Switch to workspace if we need to
	if (swayc_active_workspace() == NULL) {
		swayc_t *ws = op->children->items[0];
		workspace_switch(ws);
	}
	return true;
}

static void handle_output_destroyed(wlc_handle output) {
	int i;
	list_t *list = root_container.children;
	for (i = 0; i < list->length; ++i) {
		if (((swayc_t *)list->items[i])->handle == output) {
			break;
		}
	}
	if (i < list->length) {
		destroy_output(list->items[i]);
	} else {
		return;
	}
	if (list->length > 0) {
		// switch to other outputs active workspace
		workspace_switch(((swayc_t *)root_container.children->items[0])->focused);
	}
}

static void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
	sway_log(L_DEBUG, "Output %u resolution changed to %d x %d", (unsigned int)output, to->w, to->h);
	swayc_t *c = swayc_by_handle(output);
	if (!c) return;
	c->width = to->w;
	c->height = to->h;
	arrange_windows(&root_container, -1, -1);
}

static void handle_output_focused(wlc_handle output, bool focus) {
	swayc_t *c = swayc_by_handle(output);
	// if for some reason this output doesnt exist, create it.
	if (!c) {
		handle_output_created(output);
	}
	if (focus) {
		set_focused_container(c);
	}
}

static bool handle_view_created(wlc_handle handle) {
	// if view is child of another view, the use that as focused container
	wlc_handle parent = wlc_view_get_parent(handle);
	swayc_t *focused = NULL;
	swayc_t *newview = NULL;

	// Get parent container, to add view in
	if (parent) {
		focused = swayc_by_handle(parent);
	}
	if (!focused || focused->type == C_OUTPUT) {
		focused = get_focused_container(&root_container);
		// Move focus from floating view
		if (focused->is_floating) {
			// To workspace if there are no children
			if (focused->parent->children->length == 0) {
				focused = focused->parent;
			}
			// TODO find a better way of doing this
			// Or to focused container
			else {
				focused = get_focused_container(focused->parent->children->items[0]);
			}
		}
	}
	sway_log(L_DEBUG, "handle:%ld type:%x state:%x parent:%ld "
			"mask:%d (x:%d y:%d w:%d h:%d) title:%s "
			"class:%s appid:%s",
		handle, wlc_view_get_type(handle), wlc_view_get_state(handle), parent,
		wlc_view_get_mask(handle), wlc_view_get_geometry(handle)->origin.x,
		wlc_view_get_geometry(handle)->origin.y,wlc_view_get_geometry(handle)->size.w,
		wlc_view_get_geometry(handle)->size.h, wlc_view_get_title(handle),
		wlc_view_get_class(handle), wlc_view_get_app_id(handle));

	// TODO properly figure out how each window should be handled.
	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
		newview = new_view(focused, handle);
		wlc_view_set_state(handle, WLC_BIT_MAXIMIZED, true);
		break;

	// Dmenu keeps viewfocus, but others with this flag dont, for now simulate
	// dmenu
	case WLC_BIT_OVERRIDE_REDIRECT:
// 		locked_view_focus = true;
		wlc_view_focus(handle);
		wlc_view_set_state(handle, WLC_BIT_ACTIVATED, true);
		wlc_view_bring_to_front(handle);
		break;

	// Firefox popups have this flag set.
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		wlc_view_bring_to_front(handle);
		locked_container_focus = true;
		break;

	// Modals, get focus, popups do not
	case WLC_BIT_MODAL:
		wlc_view_focus(handle);
		wlc_view_bring_to_front(handle);
		newview = new_floating_view(handle);
	case WLC_BIT_POPUP:
		wlc_view_bring_to_front(handle);
		break;
	}

	if (newview) {
		set_focused_container(newview);
		swayc_t *output = swayc_parent_by_type(newview, C_OUTPUT);
		arrange_windows(output, -1, -1);
	}
	return true;
}

static void handle_view_destroyed(wlc_handle handle) {
	sway_log(L_DEBUG, "Destroying window %lu", handle);
	swayc_t *view = swayc_by_handle(handle);

	// destroy views by type
	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
	case WLC_BIT_MODAL:
	case WLC_BIT_POPUP:
		break;
	// DMENU has this flag, and takes view_focus, but other things with this
	// flag dont
	case WLC_BIT_OVERRIDE_REDIRECT:
// 		locked_view_focus = false;
		break;
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		locked_container_focus = false;
		break;
	}

	if (view) {
		swayc_t *parent = destroy_view(view);
		remove_view_from_scratchpad(view);
		arrange_windows(parent, -1, -1);
	}
	set_focused_container(get_focused_view(&root_container));
}

static void handle_view_focus(wlc_handle view, bool focus) {
	return;
}

static void handle_view_geometry_request(wlc_handle handle, const struct wlc_geometry *geometry) {
	sway_log(L_DEBUG, "geometry request for %ld %dx%d : %dx%d",
			handle, geometry->origin.x, geometry->origin.y, geometry->size.w, geometry->size.h);
	// If the view is floating, then apply the geometry.
	// Otherwise save the desired width/height for the view.
	// This will not do anything for the time being as WLC improperly sends geometry requests
	swayc_t *view = swayc_by_handle(handle);
	if (view) {
		view->desired_width = geometry->size.w;
		view->desired_height = geometry->size.h;

		if (view->is_floating) {
			view->width = view->desired_width;
			view->height = view->desired_height;
			view->x = geometry->origin.x;
			view->y = geometry->origin.y;
			arrange_windows(view->parent, -1, -1);
		}
	}
}

static void handle_view_state_request(wlc_handle view, enum wlc_view_state_bit state, bool toggle) {
	swayc_t *c = swayc_by_handle(view);
	switch (state) {
	case WLC_BIT_FULLSCREEN:
		// i3 just lets it become fullscreen
		wlc_view_set_state(view, state, toggle);
		if (c) {
			sway_log(L_DEBUG, "setting view %ld %s, fullscreen %d", view, c->name, toggle);
			arrange_windows(c->parent, -1, -1);
			// Set it as focused window for that workspace if its going fullscreen
			if (toggle) {
				swayc_t *ws = swayc_parent_by_type(c, C_WORKSPACE);
				// Set ws focus to c
				set_focused_container_for(ws, c);
			}
		}
		break;
	case WLC_BIT_MAXIMIZED:
	case WLC_BIT_RESIZING:
	case WLC_BIT_MOVING:
		break;
	case WLC_BIT_ACTIVATED:
		sway_log(L_DEBUG, "View %p requested to be activated", c);
		break;
	}
	return;
}


static bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t key, enum wlc_key_state state) {

	if (locked_view_focus && state == WLC_KEY_STATE_PRESSED) {
		return EVENT_PASSTHROUGH;
	}

	// reset pointer mode on keypress
	if (state == WLC_KEY_STATE_PRESSED && pointer_state.mode) {
		pointer_mode_reset();
	}

	struct sway_mode *mode = config->current_mode;

	struct wlc_modifiers no_mods = { 0, 0 };
	uint32_t sym = tolower(wlc_keyboard_get_keysym_for_key(key, &no_mods));

	int i;

	if (state == WLC_KEY_STATE_PRESSED) {
		press_key(sym, key);
	} else { // WLC_KEY_STATE_RELEASED
		release_key(sym, key);
	}

	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];

		if ((modifiers->mods ^ binding->modifiers) == 0) {
			bool match = false;
			int j;
			for (j = 0; j < binding->keys->length; ++j) {
				xkb_keysym_t *key = binding->keys->items[j];
				if ((match = check_key(*key, 0)) == false) {
					break;
				}
			}
			if (match) {
				if (state == WLC_KEY_STATE_PRESSED) {
					struct cmd_results *res = handle_command(binding->command);
					if (res->status != CMD_SUCCESS) {
						sway_log(L_ERROR, "Command '%s' failed: %s", res->input, res->error);
					}
					free_cmd_results(res);
					return EVENT_HANDLED;
				} else if (state == WLC_KEY_STATE_RELEASED) {
					// TODO: --released
				}
			}
		}
	}
	return EVENT_PASSTHROUGH;
}

static bool handle_pointer_motion(wlc_handle handle, uint32_t time, const struct wlc_origin *origin) {
	struct wlc_origin new_origin = *origin;
	// Switch to adjacent output if touching output edge.
	//
	// Since this doesn't currently support moving windows between outputs we
	// don't do the switch if the pointer is in a mode.
	if (config->seamless_mouse && !pointer_state.mode &&
			!pointer_state.left.held && !pointer_state.right.held && !pointer_state.scroll.held) {

		swayc_t *output = swayc_active_output(), *adjacent = NULL;
		if (origin->x == 0) { // Left edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_LEFT))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.x = adjacent->width;
				}
			}
		} else if ((double)origin->x == output->width) { // Right edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_RIGHT))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.x = 0;
				}
			}
		} else if (origin->y == 0) { // Top edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_UP))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.y = adjacent->height;
				}
			}
		} else if ((double)origin->y == output->height) { // Bottom edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_DOWN))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.y = 0;
				}
			}
		}
	}

	pointer_position_set(&new_origin, false);
	return EVENT_PASSTHROUGH;
}


static bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state, const struct wlc_origin *origin) {

	// Update view pointer is on
	pointer_state.view = container_under_pointer();

	// Update pointer_state
	switch (button) {
	case M_LEFT_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.left.held = true;
			pointer_state.left.x = origin->x;
			pointer_state.left.y = origin->y;
			pointer_state.left.view = pointer_state.view;
		} else {
			pointer_state.left.held = false;
		}
		break;

	case M_RIGHT_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.right.held = true;
			pointer_state.right.x = origin->x;
			pointer_state.right.y = origin->y;
			pointer_state.right.view = pointer_state.view;
		} else {
			pointer_state.right.held = false;
		}
		break;

	case M_SCROLL_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.scroll.held = true;
			pointer_state.scroll.x = origin->x;
			pointer_state.scroll.y = origin->y;
			pointer_state.scroll.view = pointer_state.view;
		} else {
			pointer_state.scroll.held = false;
		}
		break;

		//TODO scrolling behavior
	case M_SCROLL_UP:
	case M_SCROLL_DOWN:
		break;
	}

	// get focused window and check if to change focus on mouse click
	swayc_t *focused = get_focused_container(&root_container);

	// dont change focus or mode if fullscreen
	if (swayc_is_fullscreen(focused)) {
		return EVENT_PASSTHROUGH;
	}

	// set pointer mode only if floating mod has been set
	if (config->floating_mod) {
		pointer_mode_set(button, !(modifiers->mods ^ config->floating_mod));
	}

	// Check whether to change focus
	swayc_t *pointer = pointer_state.view;
	if (pointer) {
		if (focused != pointer) {
			set_focused_container(pointer_state.view);
		}
		// Send to front if floating
		if (pointer->is_floating) {
			int i;
			for (i = 0; i < pointer->parent->floating->length; i++) {
				if (pointer->parent->floating->items[i] == pointer) {
					list_del(pointer->parent->floating, i);
					list_add(pointer->parent->floating, pointer);
					break;
				}
			}
			wlc_view_bring_to_front(pointer->handle);
		}
	}

	// Return if mode has been set
	if (pointer_state.mode) {
		return EVENT_HANDLED;
	}

	// Always send mouse release
	if (state == WLC_BUTTON_STATE_RELEASED) {
		return EVENT_PASSTHROUGH;
	}

	// Finally send click
	return EVENT_PASSTHROUGH;
}

static void handle_wlc_ready(void) {
	sway_log(L_DEBUG, "Compositor is ready, executing cmds in queue");
	// Execute commands until there are none left
	config->active = true;
	while (config->cmd_queue->length) {
		char *line = config->cmd_queue->items[0];
		struct cmd_results *res = handle_command(line);
		if (res->status != CMD_SUCCESS) {
			sway_log(L_ERROR, "Error on line '%s': %s", line, res->error);
		}
		free_cmd_results(res);
		free(line);
		list_del(config->cmd_queue, 0);
	}
}

struct wlc_interface interface = {
	.output = {
		.created = handle_output_created,
		.destroyed = handle_output_destroyed,
		.resolution = handle_output_resolution_change,
		.focus = handle_output_focused
	},
	.view = {
		.created = handle_view_created,
		.destroyed = handle_view_destroyed,
		.focus = handle_view_focus,
		.request = {
			.geometry = handle_view_geometry_request,
			.state = handle_view_state_request
		}
	},
	.keyboard = {
		.key = handle_key
	},
	.pointer = {
		.motion = handle_pointer_motion,
		.button = handle_pointer_button
	},
	.compositor = {
		.ready = handle_wlc_ready
	}
};
