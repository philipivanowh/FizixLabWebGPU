#pragma once
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
//  KeybindUI  –  drop-in ImGui keybind selection widget
//
//  Usage:
//    1. Call KeybindUI::PollKeys(window) once per frame (before ImGui::NewFrame
//       or in your GLFW key callback).
//    2. Replace your key-display snippet with KeybindUI::Widget(...).
//    3. Optionally call KeybindUI::Panel(...) for a full keybind settings panel.
// ─────────────────────────────────────────────────────────────────────────────

namespace KeybindUI
{
    // ── Internal state ────────────────────────────────────────────────────────
    namespace detail
    {
        struct ListenState
        {
            bool     active   = false;
            int     *target   = nullptr;   // pointer to the int being rebound
            double   startT   = 0.0;
            float    timeout  = 5.0f;      // seconds before auto-cancel
        };

        inline ListenState   g_listen;
        inline int           g_lastKey    = GLFW_KEY_UNKNOWN;
        inline bool          g_keyPressed = false;

        // Pretty names for keys that glfwGetKeyName returns nullptr for
        inline const char *FallbackName(int key)
        {
            switch (key)
            {
            case GLFW_KEY_SPACE:         return "Space";
            case GLFW_KEY_ESCAPE:        return "Esc";
            case GLFW_KEY_ENTER:         return "Enter";
            case GLFW_KEY_TAB:           return "Tab";
            case GLFW_KEY_BACKSPACE:     return "Backspace";
            case GLFW_KEY_INSERT:        return "Insert";
            case GLFW_KEY_DELETE:        return "Delete";
            case GLFW_KEY_RIGHT:         return "Right";
            case GLFW_KEY_LEFT:          return "Left";
            case GLFW_KEY_DOWN:          return "Down";
            case GLFW_KEY_UP:            return "Up";
            case GLFW_KEY_PAGE_UP:       return "PgUp";
            case GLFW_KEY_PAGE_DOWN:     return "PgDn";
            case GLFW_KEY_HOME:          return "Home";
            case GLFW_KEY_END:           return "End";
            case GLFW_KEY_CAPS_LOCK:     return "Caps";
            case GLFW_KEY_LEFT_SHIFT:    return "L.Shift";
            case GLFW_KEY_RIGHT_SHIFT:   return "R.Shift";
            case GLFW_KEY_LEFT_CONTROL:  return "L.Ctrl";
            case GLFW_KEY_RIGHT_CONTROL: return "R.Ctrl";
            case GLFW_KEY_LEFT_ALT:      return "L.Alt";
            case GLFW_KEY_RIGHT_ALT:     return "R.Alt";
            case GLFW_KEY_LEFT_SUPER:    return "L.Super";
            case GLFW_KEY_RIGHT_SUPER:   return "R.Super";
            case GLFW_KEY_F1:  return "F1";  case GLFW_KEY_F2:  return "F2";
            case GLFW_KEY_F3:  return "F3";  case GLFW_KEY_F4:  return "F4";
            case GLFW_KEY_F5:  return "F5";  case GLFW_KEY_F6:  return "F6";
            case GLFW_KEY_F7:  return "F7";  case GLFW_KEY_F8:  return "F8";
            case GLFW_KEY_F9:  return "F9";  case GLFW_KEY_F10: return "F10";
            case GLFW_KEY_F11: return "F11"; case GLFW_KEY_F12: return "F12";
            case GLFW_KEY_KP_0: return "KP0"; case GLFW_KEY_KP_1: return "KP1";
            case GLFW_KEY_KP_2: return "KP2"; case GLFW_KEY_KP_3: return "KP3";
            case GLFW_KEY_KP_4: return "KP4"; case GLFW_KEY_KP_5: return "KP5";
            case GLFW_KEY_KP_6: return "KP6"; case GLFW_KEY_KP_7: return "KP7";
            case GLFW_KEY_KP_8: return "KP8"; case GLFW_KEY_KP_9: return "KP9";
            case GLFW_KEY_KP_ENTER:    return "KP Enter";
            case GLFW_KEY_KP_ADD:      return "KP +";
            case GLFW_KEY_KP_SUBTRACT: return "KP -";
            case GLFW_KEY_KP_MULTIPLY: return "KP *";
            case GLFW_KEY_KP_DIVIDE:   return "KP /";
            default:
            {
                static char buf[16];
                snprintf(buf, sizeof(buf), "Key %d", key);
                return buf;
            }
            }
        }

        inline std::string KeyLabel(int key)
        {
            if (key == GLFW_KEY_UNKNOWN) return "---";
            const char *n = glfwGetKeyName(key, 0);
            if (n && n[0]) {
                // Capitalise single-char names
                std::string s(n);
                if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z')
                    s[0] -= 32;
                return s;
            }
            return FallbackName(key);
        }
    } // namespace detail

    // ── Call once per frame in your GLFW key callback (or before ImGui::Render)
    //    glfwSetKeyCallback should forward to this.
    inline void OnKeyEvent(int key, int /*scancode*/, int action, int /*mods*/)
    {
        if (action == GLFW_PRESS && key != GLFW_KEY_UNKNOWN)
        {
            detail::g_lastKey    = key;
            detail::g_keyPressed = true;
        }
    }

    // ── Alternatively, poll every frame if you own the GLFW window ──────────
    inline void PollKeys(GLFWwindow *window)
    {
        // Scan a reasonable range of keys each frame
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k)
        {
            if (glfwGetKey(window, k) == GLFW_PRESS)
            {
                if (detail::g_lastKey != k) {
                    detail::g_lastKey    = k;
                    detail::g_keyPressed = true;
                }
                return;
            }
        }
    }

    // ── Must be called once per frame (after polling / in your ImGui loop) ───
    inline void Update()
    {
        auto &ls = detail::g_listen;
        if (!ls.active) { detail::g_keyPressed = false; return; }

        float elapsed = (float)(glfwGetTime() - ls.startT);

        if (detail::g_keyPressed && detail::g_lastKey != GLFW_KEY_UNKNOWN)
        {
            if (detail::g_lastKey == GLFW_KEY_ESCAPE)
                ls = {};   // cancel
            else if (ls.target)
                *ls.target = detail::g_lastKey;
            ls = {};
        }
        else if (elapsed >= ls.timeout)
            ls = {};       // timed out

        detail::g_keyPressed = false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Widget  –  inline badge + click-to-rebind button
    //
    //  label      : left-side label (e.g. "Fire Key")
    //  key        : reference to the int storing the current GLFW key
    //  labelWidth : pixel offset before the badge (match your existing 48.0f)
    //  pulse      : optional brightness pulse 0..1 (pass your existing value)
    // ─────────────────────────────────────────────────────────────────────────
    inline void Widget(const char *label, int &key,
                       float labelWidth = 92.0f,
                       float pulse      = 1.0f)
    {
        auto      &ls       = detail::g_listen;
        bool       listening = ls.active && ls.target == &key;
        std::string keyStr  = detail::KeyLabel(key);

        // ── Label ────────────────────────────────────────────────────────────
        ImGui::TextColored({0.55f, 0.55f, 0.55f, 1.0f}, "  %s", label);
        ImGui::SameLine(labelWidth);

        // ── Badge ─────────────────────────────────────────────────────────────
        if (listening)
        {
            float remaining = ls.timeout - (float)(glfwGetTime() - ls.startT);
            float blink     = 0.6f + 0.4f * sinf((float)glfwGetTime() * 8.0f);

            ImGui::TextColored({1.0f, 0.8f, 0.2f, blink},
                               "[ press a key… %.0fs ]", remaining);
            ImGui::SameLine();
            if (ImGui::SmallButton("✕"))
                ls = {};
        }
        else
        {
            // Coloured key badge
            ImVec4 badgeCol = {0.25f * pulse, 0.85f * pulse, 0.45f * pulse, 1.0f};
            ImGui::TextColored(badgeCol, "[ %s ]", keyStr.c_str());

            // Hover → show rebind hint
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Click to rebind");
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            // Click → enter listen mode
            if (ImGui::IsItemClicked())
            {
                ls.active  = true;
                ls.target  = &key;
                ls.startT  = glfwGetTime();
            }

            // Small edit button beside the badge
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.18f,0.18f,0.22f,1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f,0.28f,0.35f,1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.20f,0.50f,0.30f,1.f});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {3.f, 1.f});

            char btnId[64];
            snprintf(btnId, sizeof(btnId), "##rebind_%s", label);
            if (ImGui::SmallButton(btnId[0] ? "✎" : "✎"))
            {
                ls.active = true;
                ls.target = &key;
                ls.startT = glfwGetTime();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rebind key");

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Panel  –  standalone "Keybinds" settings window
    //
    //  Entries is a list of { display name, key reference } pairs.
    //
    //  Example:
    //    KeybindUI::PanelEntry entries[] = {
    //        { "Fire",         thruster->fireKey  },
    //        { "Boost",        thruster->boostKey },
    //    };
    //    KeybindUI::Panel("Thruster Keybinds", entries, 2);
    // ─────────────────────────────────────────────────────────────────────────
    struct PanelEntry
    {
        const char *label;
        int        &key;
    };

    inline void Panel(const char    *title,
                      PanelEntry    *entries,
                      int            count,
                      bool          *p_open   = nullptr,
                      float          width    = 300.0f)
    {
        ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({width, 80}, {width, 800});

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar;
        if (!ImGui::Begin(title, p_open, flags))
        { ImGui::End(); return; }

        // Header strip
        ImGui::PushStyleColor(ImGuiCol_Separator, {0.25f,0.70f,0.40f,0.6f});
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Column headers
        constexpr float kLabelW = 100.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, {0.45f,0.45f,0.50f,1.f});
        ImGui::Text("  Action");
        ImGui::SameLine(kLabelW);
        ImGui::Text("Binding");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            Widget(entries[i].label, entries[i].key, kLabelW);
            ImGui::Spacing();
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Reset all button
        ImGui::Spacing();
        ImGui::SetCursorPosX((width - 110) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.20f,0.20f,0.25f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.35f,0.20f,0.20f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.60f,0.15f,0.15f,1.f});
        if (ImGui::Button("Reset to defaults", {110,0}))
        {
            // User should implement their own reset; this is a no-op placeholder.
            // e.g. for (auto &e : entries) e.key = defaultKeys[i];
        }
        ImGui::PopStyleColor(3);
        ImGui::Spacing();

        ImGui::End();
    }

} // namespace KeybindUI