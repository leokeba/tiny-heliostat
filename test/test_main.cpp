// Host test runner: ./heliostat_tests [filter]
#include "sun_cases.h"
#include "geometry_cases.h"
#include "tracker_cases.h"
#include "artnet_cases.h"
#include "tree_cases.h"

int main(int argc, char** argv) { return check::run(argc > 1 ? argv[1] : nullptr) ? 1 : 0; }
