/* Regression for ac301fb: hidden inventory descendants remain identifiable. */

/* Compile the existing UI translation unit; do not copy its widget-chain code. */
#include "../src/GAME2.c"

enum { WIDGET_PARENT_OFFSET = 396, WIDGET_SIZE = 400 };

static void set_parent(unsigned char *widget, void *parent)
{
    *(void **)(widget + WIDGET_PARENT_OFFSET) = parent;
}

int main(void)
{
    unsigned char root[WIDGET_SIZE] = {0};
    unsigned char child[WIDGET_SIZE] = {0};
    unsigned char sibling[WIDGET_SIZE] = {0};

    set_parent(child, root);

    if (!ui_chain_contains(child, root))
        return 1;
    if (!ui_chain_contains(root, root))
        return 1;
    if (ui_chain_contains(sibling, root))
        return 1;
    if (ui_chain_contains(NULL, root))
        return 1;

    return 0;
}
