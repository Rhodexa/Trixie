// wm.cpp
// Window manager: event translation, keymap matching, operator dispatch.

#include "wm.h"
#include "piano_roll_space.h"  // complete SpacePianoRoll for active_modal_op

#include <GLFW/glfw3.h>

// ── Event translation ─────────────────────────────────────────────────────────

wmEvent wm_event_from_input(const InputEvent& raw) {
    return std::visit([](auto&& e) -> wmEvent {
        using T = std::decay_t<decltype(e)>;
        wmEvent ev;

        if constexpr (std::is_same_v<T, ScrollEvent>) {
            ev.type = (e.dy >= 0.0f) ? EventType::WheelUp : EventType::WheelDown;
            ev.mods = e.mods;
            ev.x = e.mx;  ev.y  = e.my;
            ev.dx = e.dx; ev.dy = e.dy;

        } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
            switch (e.button) {
                case GLFW_MOUSE_BUTTON_LEFT:   ev.type = EventType::LeftMouse;   break;
                case GLFW_MOUSE_BUTTON_RIGHT:  ev.type = EventType::RightMouse;  break;
                case GLFW_MOUSE_BUTTON_MIDDLE: ev.type = EventType::MiddleMouse; break;
                default:                       ev.type = EventType::None;        break;
            }
            ev.value = e.pressed ? EventValue::Press : EventValue::Release;
            ev.mods  = e.mods;
            ev.x = e.x; ev.y = e.y;

        } else if constexpr (std::is_same_v<T, MouseMoveEvent>) {
            ev.type = EventType::MouseMove;
            ev.x = e.x;   ev.y  = e.y;
            ev.dx = e.dx; ev.dy = e.dy;

        } else if constexpr (std::is_same_v<T, KeyEvent>) {
            ev.type  = EventType::Key;
            ev.value = (e.action == GLFW_PRESS || e.action == GLFW_REPEAT)
                       ? EventValue::Press : EventValue::Release;
            ev.mods  = e.mods;
            ev.key   = e.key;
        }

        return ev;
    }, raw);
}

// ── Keymap matching ───────────────────────────────────────────────────────────

bool wm_event_matches(const wmEvent& ev, const wmKeyMapItem& item) {
    if (ev.type != item.type)
        return false;
    // value = Nothing means "don't check value" (used for scroll/move)
    if (item.value != EventValue::Nothing && ev.value != item.value)
        return false;
    // Subset mod match: all required mods must be present
    if ((ev.mods & item.mods) != item.mods)
        return false;
    // Key check only for Key events, and only when item specifies one
    if (ev.type == EventType::Key && item.key != 0 && ev.key != item.key)
        return false;
    return true;
}

// ── Operator dispatch ─────────────────────────────────────────────────────────

bool wm_handle_event(wmOpContext& ctx, const wmEvent& ev,
                     const wmKeyMap& keymap,
                     std::optional<wmOperator>& active_modal_op) {
    // ── Modal phase ───────────────────────────────────────────────────────────
    // The active modal op owns all events until it exits.
    if (active_modal_op) {
        wmOperator& op = *active_modal_op;
        OpResult r = op.type->modal(op, ctx, ev);
        if (r != OpResult::Running) {
            if (r == OpResult::Cancelled && op.type->cancel)
                op.type->cancel(op, ctx);
            active_modal_op.reset();
        }
        return true;
    }

    // ── Invoke phase ──────────────────────────────────────────────────────────
    // Walk the keymap in order; first non-Pass result wins.
    for (int i = 0; i < keymap.count; i++) {
        const wmKeyMapItem& item = keymap.items[i];
        if (!wm_event_matches(ev, item)) continue;
        if (item.op->poll && !item.op->poll(ctx)) continue;

        wmOperator op{ item.op, nullptr };
        OpResult r = item.op->invoke(op, ctx, ev);

        if (r == OpResult::Running)
            active_modal_op = std::move(op);

        if (r != OpResult::Pass)
            return true;   // consumed (Finished / Cancelled / Running)
        // Pass → fall through to next item
    }

    return false;  // nothing handled it
}
