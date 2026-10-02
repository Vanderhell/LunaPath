/* SPDX-License-Identifier: Apache-2.0 */
#include <lunapath.h>

int main() {
    lunapath_path path{};
    lunapath_path_zero(&path);
    lunapath_path child{};
    return lunapath_child(&path, true, &child) && child.depth == 1u ? 0 : 1;
}
