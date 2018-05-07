/* TinyWM is written by Nick Welch <mack@incise.org>, 2005.
 * ittywm is written by Marshall Mason <marshallmason2 at gmail.com>, 2018.
 *
 * This software is in the public domain
 * and is provided AS IS, with NO WARRANTY. */

#include <stdlib.h>
#include <xcb/xcb.h>

xcb_connection_t *dpy;
xcb_screen_t *screen;

// Connect to X server, initialize screen, grab keys and buttons
void setup(void)
{
    int screen_num;
    dpy = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(dpy)) exit(1);

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(dpy));

    for (int i = 0; i < screen_num; i++)
        xcb_screen_next(&iter);

    screen = iter.data;

    xcb_grab_key(dpy, 1, screen->root, XCB_MOD_MASK_1, XCB_NO_SYMBOL,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_button(dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS | 
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, XCB_MOD_MASK_1);
    xcb_grab_button(dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS | 
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 3, XCB_MOD_MASK_1);
}

// Raises window and sets input focus
void raise_window(xcb_window_t window)
{
    const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(dpy, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_PARENT, window, XCB_CURRENT_TIME);
}

// Creates a clone of a window, used for resizing
xcb_window_t duplicate_window(xcb_window_t window, xcb_get_geometry_reply_t *geom)
{
    const xcb_window_t duplicate = xcb_generate_id(dpy);

    const uint32_t values[] = { 0 };
    xcb_create_window(dpy, XCB_COPY_FROM_PARENT, duplicate, screen->root,
        geom->x, geom->y, geom->width, geom->height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual, XCB_CW_BACK_PIXEL, values);

    return duplicate;
}

void motion_notify(xcb_window_t window, xcb_button_t button, xcb_get_geometry_reply_t *geom, int16_t *x, int16_t *y)
{
    // Update pointer location
    xcb_query_pointer_reply_t *pointer;
    pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, screen->root), 0);
    if (!pointer) return;
    const int16_t diffx = pointer->root_x - *x;
    const int16_t diffy = pointer->root_y - *y;
    free(pointer);
    *x += diffx;
    *y += diffy;

    // Update geometry and prepare values for moving or resizing
    uint16_t value_mask;
    uint32_t value_list[2];
    if (button == 1) // Move
    {
        geom->x += diffx;
        geom->y += diffy;
        value_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
        value_list[0] = geom->x;
        value_list[1] = geom->y;
    }
    else // Resize
    {
        // Prevent underflow and require a minimum size of 32x32
        if (geom->width + diffx < 32)
            geom->width = 32;
        else
            geom->width += diffx;
        if (geom->height + diffx < 32)
            geom->height = 32;
        else
            geom->height += diffy;
        value_mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        value_list[0] = geom->width;
        value_list[1] = geom->height;
    }

    // Performs moving or resizing based on values set
    xcb_configure_window(dpy, window, value_mask, value_list);
}

// Replace the phony window with the real one, and complete the resize
void replace_window(xcb_window_t window, xcb_window_t phony, const xcb_get_geometry_reply_t *geom)
{
    xcb_destroy_window(dpy, phony);
    const uint16_t value_mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t value_list[] = { geom->width, geom->height };
    xcb_configure_window(dpy, window, value_mask, value_list);
    xcb_map_window(dpy, window);
    raise_window(window);
}

// Handle mouse button press event by handling motion and release events
void button_press(xcb_button_press_event_t *e)
{
    // Record initial window geometry
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, e->child), NULL);
    if (!geom) return;

    // If resizing, replace with an opaque window to prevent sluggish redrawing while in motion
    xcb_window_t window;
    if (e->detail == 1) // Move
    {
        window = e->child;
    }
    else
    {
        window = duplicate_window(e->child, geom);
        xcb_unmap_window(dpy, e->child);
        xcb_map_window(dpy, window);
    }

    raise_window(window);

    // Listen for motion and button release events
    xcb_grab_pointer(dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_RELEASE
        | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, 
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, XCB_CURRENT_TIME);

    // Record initial pointer location
    xcb_query_pointer_reply_t *pointer;
    pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, screen->root), 0);
    if (!pointer) return;
    int16_t x = pointer->root_x;
    int16_t y = pointer->root_y;
    free(pointer);

    // Update the window as motion is made and button is released
    int done = 0;
    do
    {
        xcb_flush(dpy);
        xcb_generic_event_t *ev = xcb_wait_for_event(dpy);

        switch (ev->response_type & ~0x80)
	{
        case XCB_MOTION_NOTIFY:
            motion_notify(window, e->detail, geom, &x, &y);
            break;

        case XCB_BUTTON_RELEASE:
            xcb_ungrab_pointer(dpy, XCB_CURRENT_TIME);
            if (e->detail != 1) replace_window(e->child, window, geom);
            done = 1;
            break;
        }
    } while (!done);

    free(geom);
}

// Handle key press by raising the window, the only action available
void key_press(xcb_key_press_event_t *e)
{
    raise_window(e->child);
}

// Initialize and run the main event loop
int main(void)
{
    setup();

    for (;;)
    {
        xcb_flush(dpy);

        xcb_generic_event_t *ev = xcb_wait_for_event(dpy);
        switch (ev->response_type & ~0x80)
	{
        case XCB_KEY_PRESS:
            key_press((xcb_key_press_event_t *) ev);
            break;
        case XCB_BUTTON_PRESS:
            button_press((xcb_button_press_event_t *) ev);
            break;
        }
        free(ev);
    }
}
