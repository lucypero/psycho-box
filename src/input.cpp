#include "input.hpp"

#include "utils.hpp"

void Input::update() {
    // keyboard/mouse update
    this->mouse_last_pos_x = this->mouse_cur_pos_x;
    this->mouse_last_pos_y = this->mouse_cur_pos_y;

    // gotta reset the scroll bc there is no scroll down event
    this->keys[(size_t)Key::SCROLL_UP] = false;
    this->keys[(size_t)Key::SCROLL_DOWN] = false;

    memcpy(this->keys_prev, this->keys, sizeof(bool) * (size_t)Key::COUNT);

    // xinput
    XINPUT_STATE state = {};
    DWORD dwResult;

    dwResult = XInputGetState(0, &state);

    if (dwResult == ERROR_SUCCESS) {
        this->joy_state_prev = this->joy_state;
        this->joy_state = state;
    } else {
        this->joy_state_prev = {};
        this->joy_state = {};
    }
}

bool Input::is_down(Key key) {
    return this->keys[(size_t)key];
}

bool Input::was_up(Key key) {
    return this->keys[(size_t)key] && !this->keys_prev[(size_t)key];
}

bool Input::was_down(Key key) {
    return !this->keys[(size_t)key] && this->keys_prev[(size_t)key];
}

bool Input::mouse_moved() {
    return (this->mouse_last_pos_x != this->mouse_cur_pos_x) || (this->mouse_last_pos_y != this->mouse_cur_pos_y);
}

void Input::action_new(Action action) {
    ActionMap am = {};
    am.action = action;

    this->actions.push_back(am);
}

void Input::action_add_key(Action action, Key key) {
    bool found = false;

    for (auto &am : this->actions) {
        if (am.action != action) {
            continue;
        }

        am.keys.push_back(key);
        found = true;
        break;
    }

    lassert(found);
}

void Input::action_add_joy_button(Action action, JoyButton button) {
    bool found = false;

    for (auto &am : this->actions) {
        if (am.action != action) {
            continue;
        }

        am.joy_buttons.push_back(button);
        found = true;
        break;
    }

    lassert(found);
}

bool Input::was_up(JoyButton button) {
    bool pressed_now = (this->joy_state.Gamepad.wButtons & (WORD)button) != 0;
    bool pressed_before = (this->joy_state_prev.Gamepad.wButtons & (WORD)button) != 0;

    return pressed_now && !pressed_before;
}

bool Input::was_up(Action action) {
    bool found = false;

    for (auto &am : this->actions) {
        if (am.action != action) {
            continue;
        }

        for (const auto &key : am.keys) {
            if (this->was_up(key))
                return true;
        }

        for (const auto &button : am.joy_buttons) {
            if (this->was_up(button))
                return true;
        }

        return false;
    }

    lassert(found);
    return false;
}