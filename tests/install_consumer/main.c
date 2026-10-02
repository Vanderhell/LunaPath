/* SPDX-License-Identifier: Apache-2.0 */
#include <lunapath.h>

int main(void) {
    lunapath_state state;
    lunapath_state_zero(&state);
    if (!lunapath_step(&state, true, NULL, 0u)) return 1;
    return state.path.depth == 1u ? 0 : 2;
}
