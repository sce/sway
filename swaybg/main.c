#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <time.h>
#include "client.h"
#include "log.h"

struct client_state *state;

void sway_terminate(void) {
	client_teardown(state);
	exit(1);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	if (!(state = client_setup())) {
		return -1;
	}

	int rs = 0;
	do {
		rs = wl_display_dispatch(state->display);
	} while (rs != -1);

	client_teardown(state);
	return 0;
}
