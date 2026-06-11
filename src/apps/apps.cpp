#include "apps.h"

#include "launcher/registry.h"
#include "counter/counter.h"
#include "events/events.h"
#include "home/home.h"
#include "scoreboard/scoreboard.h"
#include "settings/settings.h"
#include "stopwatch/stopwatch.h"
#include "todo/todo.h"

// Add one line per app. Home-carousel order = order of frij_registry_add();
// Home is first so it's the default landing glance. Settings registers in its
// own slot (reached by swiping down).
void frij_register_apps(void)
{
    frij_registry_add(home_app());
    frij_registry_add(todo_app());
    frij_registry_add(events_app());
    frij_registry_add(stopwatch_app());
    frij_registry_add(scoreboard_app());
    frij_registry_add(counter_app());
    frij_registry_set_settings(settings_app());
}
