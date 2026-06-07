#include "apps.h"

#include "launcher/registry.h"
#include "counter/counter.h"
#include "todo/todo.h"

// Add one line per app. Order here = order on the home screen.
void frij_register_apps(void)
{
    frij_registry_add(todo_app());
    frij_registry_add(counter_app());
}
