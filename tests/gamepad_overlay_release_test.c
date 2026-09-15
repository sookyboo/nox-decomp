/* Regression for 7ff93d5: removing an overlay must clear held-input state for
 * the removed layer and every child removed with it. */
#include "../src/gamepad.c"

static void reset_fixture(void)
{
    memset(g_layers, 0, sizeof(g_layers));
    memset(g_active_stack, 0, sizeof(g_active_stack));
    memset(g_hold_layer_for_in, 0xff, sizeof(g_hold_layer_for_in));
    g_layer_count = 1; /* base layer */
    g_active_count = 0;
    g_set_layer_idx = -1;
}

static int add_layer(const char *name)
{
    strncpy(g_layers[g_layer_count].name, name,
            sizeof(g_layers[g_layer_count].name) - 1);
    return g_layer_count++;
}

int main(void)
{
    int parent;
    int child;

    reset_fixture();
    parent = add_layer("parent");
    child = add_layer("child");

    /* This is the production hold sequence: child is parented to parent. */
    stack_push_with_parent(parent, -1);
    stack_push_with_parent(child, 0);
    g_hold_layer_for_in[IN_L1] = parent;
    g_hold_layer_for_in[IN_R1] = child;
    if (g_active_count != 2)
        return 1;

    /* Releasing the parent removes its child and clears both held bindings. */
    stack_remove_layer(parent);
    if (g_active_count != 0 ||
        g_hold_layer_for_in[IN_L1] != -1 ||
        g_hold_layer_for_in[IN_R1] != -1)
        return 2;

    /* A repeated release must not remove or release anything a second time. */
    stack_remove_layer(parent);
    if (g_active_count != 0)
        return 3;

    /* The same physical inputs can create both overlays again after release. */
    stack_push_with_parent(parent, -1);
    stack_push_with_parent(child, 0);
    if (g_active_count != 2)
        return 4;

    return 0;
}
