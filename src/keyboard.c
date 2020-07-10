#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "keyboard.h"

struct keyboard* keyboard_new(struct wl_keyboard* wl_keyboard)
{
	struct keyboard* self = calloc(1, sizeof(*self));
	if (!self)
		return NULL;

	self->wl_keyboard = wl_keyboard;
	self->context = xkb_context_new(0);

	return self;
}

void keyboard_destroy(struct keyboard* self)
{
	if (self->state)
		xkb_state_unref(self->state);

	if (self->keymap)
		xkb_keymap_unref(self->keymap);

	xkb_context_unref(self->context);
	wl_keyboard_destroy(self->wl_keyboard);
	free(self);
}

struct keyboard_collection* keyboard_collection_new(void)
{
	struct keyboard_collection* self = calloc(1, sizeof(*self));
	if (!self)
		return NULL;

	wl_list_init(&self->keyboards);

	return self;
}

void keyboard_collection_destroy(struct keyboard_collection* self)
{
	struct keyboard* keyboard;
	struct keyboard* tmp;

	wl_list_for_each_safe(keyboard, tmp, &self->keyboards, link) {
		wl_list_remove(&keyboard->link);
		keyboard_destroy(keyboard);
	}

	free(self);
}

struct keyboard* keyboard_collection_find_wl_keyboard(
		struct keyboard_collection* self, struct wl_keyboard* wl_keyboard)
{
	struct keyboard* keyboard;
	wl_list_for_each(keyboard, &self->keyboards, link)
		if (keyboard->wl_keyboard == wl_keyboard)
			return keyboard;

	return NULL;
}

static void keyboard_keymap(void* data, struct wl_keyboard* wl_keyboard,
		enum wl_keyboard_keymap_format format, int32_t fd,
		uint32_t size)
{
	struct keyboard_collection* self = data;
	struct keyboard* keyboard =
		keyboard_collection_find_wl_keyboard(self, wl_keyboard);
	assert(keyboard);

	// TODO: Ignore keyboard if no proper format is available?
	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

	char* buffer = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(buffer);

	if (keyboard->state)
		xkb_state_unref(keyboard->state);

	if (keyboard->keymap)
		xkb_keymap_unref(keyboard->keymap);

	keyboard->keymap = xkb_keymap_new_from_string(keyboard->context,
			buffer, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
	assert(keyboard->keymap);

	munmap(buffer, size);

	keyboard->state = xkb_state_new(keyboard->keymap);
	assert(keyboard->state);
}

static void keyboard_enter(void* data, struct wl_keyboard* wl_keyboard,
		uint32_t serial, struct wl_surface* surface,
		struct wl_array* keys)
{
	// TODO
}

static void keyboard_leave(void* data, struct wl_keyboard* wl_keyboard,
		uint32_t serial, struct wl_surface* surface)
{
	// TODO
}

static enum xkb_key_direction xbk_key_direction_from_wl_keyboard_key_state(
		enum wl_keyboard_key_state state)
{
	switch (state) {
	case WL_KEYBOARD_KEY_STATE_PRESSED:
		return XKB_KEY_DOWN;
	case WL_KEYBOARD_KEY_STATE_RELEASED:
		return XKB_KEY_UP;
	}

	abort();
	return 0;
}

static void keyboard_key(void* data, struct wl_keyboard* wl_keyboard,
		uint32_t serial, uint32_t t, uint32_t key,
		enum wl_keyboard_key_state state)
{
	struct keyboard_collection* self = data;
	struct keyboard* keyboard =
		keyboard_collection_find_wl_keyboard(self, wl_keyboard);
	assert(keyboard);

	enum xkb_key_direction dir =
		xbk_key_direction_from_wl_keyboard_key_state(state);
	xkb_state_update_key(keyboard->state, key + 8, dir);

	self->on_event(self, keyboard, key + 8,
			state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

static void keyboard_modifiers(void* data, struct wl_keyboard* wl_keyboard,
		uint32_t serial, uint32_t depressed, uint32_t latched,
		uint32_t locked, uint32_t group)
{
	struct keyboard_collection* self = data;
	struct keyboard* keyboard =
		keyboard_collection_find_wl_keyboard(self, wl_keyboard);
	assert(keyboard);

	xkb_state_update_mask(keyboard->state, depressed, latched, locked, 0, 0,
			group);
}

static void keyboard_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
		int32_t rate, int32_t delay)
{
	// TODO
}

static struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

int keyboard_collection_add_wl_keyboard(struct keyboard_collection* self,
		struct wl_keyboard* wl_keyboard)
{
	struct keyboard* keyboard = keyboard_new(wl_keyboard);
	if (!keyboard)
		return -1;

	wl_list_insert(&self->keyboards, &keyboard->link);
	wl_keyboard_add_listener(keyboard->wl_keyboard, &keyboard_listener, self);
	return 0;
}

void keyboard_collection_remove_wl_keyboard(struct keyboard_collection* self,
	struct wl_keyboard* wl_keyboard)
{
	struct keyboard* keyboard =
		keyboard_collection_find_wl_keyboard(self, wl_keyboard);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}
