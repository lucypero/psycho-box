#pragma once

#include "lucytypes.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>

// keyboard keys, mouse buttons, scroll down and scroll up
enum struct Key {
    Q,
    W,
    E,
    R,
    T,
    Y,
    U,
    I,
    O,
    P,
    A,
    S,
    D,
    F,
    G,
    H,
    J,
    K,
    L,
    Z,
    X,
    C,
    V,
    B,
    N,
    M,
    LEFT,
    DOWN,
    UP,
    RIGHT,
    NUM_1,
    NUM_2,
    NUM_3,
    NUM_4,
    NUM_5,
    NUM_6,
    NUM_7,
    NUM_8,
    NUM_9,
    NUM_0,
    NP_1,
    NP_2,
    NP_3,
    NP_4,
    NP_5,
    NP_6,
    NP_7,
    NP_8,
    NP_9,
    NP_0,
    MOUSE_1,
    MOUSE_2,
    SCROLL_DOWN,
    SCROLL_UP,
    SPACE,
    ESCAPE,
    ENTER,
    SQUARE_LEFT,
    SQUARE_RIGHT,

    COUNT
};

enum struct JoyButton {
    A = XINPUT_GAMEPAD_A,
    B = XINPUT_GAMEPAD_B,
    X = XINPUT_GAMEPAD_X,
    Y = XINPUT_GAMEPAD_Y,
    BACK = XINPUT_GAMEPAD_BACK,
    START = XINPUT_GAMEPAD_START,
    LS = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RS = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    DPAD_UP = XINPUT_GAMEPAD_DPAD_UP,
    DPAD_DOWN = XINPUT_GAMEPAD_DPAD_DOWN,
    DPAD_LEFT = XINPUT_GAMEPAD_DPAD_LEFT,
    DPAD_RIGHT = XINPUT_GAMEPAD_DPAD_RIGHT,
};

enum struct Action : u32 {
    Jump,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Undo,
    Reset,
    Back,
    CameraLeft,
    CameraRight,
};

struct ActionMap {
    Action action;
    vec<Key> keys;
    vec<JoyButton> joy_buttons;
};

struct Input {
    // keyboard state
    bool keys_prev[(size_t)Key::COUNT];
    bool keys[(size_t)Key::COUNT];

    // mouse cursor state
    int mouse_last_pos_x;
    int mouse_last_pos_y;
    int mouse_cur_pos_x;
    int mouse_cur_pos_y;

    // controller state
    XINPUT_STATE joy_state_prev;
    XINPUT_STATE joy_state;

    vec<ActionMap> actions;

    bool is_down(Key key);

    // if the key went from up to down in this frame
    bool was_up(Key key);
    bool was_up(JoyButton button);
    bool was_up(Action action);

    // if the key went from down to up in this frame
    bool was_down(Key key);

    bool mouse_moved();

    void action_new(Action action);
    void action_add_key(Action action, Key key);
    void action_add_joy_button(Action action, JoyButton button);

    void update();
};