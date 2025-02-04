/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2019
 *   Author(s): Meng Tan
 */

#include "mod/internal/transition_mod.hpp"

TransitionMod::TransitionMod(
    char const * message,
    gdi::GraphicApi & drawable,
    uint16_t width, uint16_t height,
    Rect const widget_rect, ClientExecute & rail_client_execute, Font const& font,
    Theme const& theme
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, nullptr)
    , ttmessage(drawable, message,
                theme.tooltip.fgcolor, theme.tooltip.bgcolor,
                theme.tooltip.border_color, font)
{
    Dimension dim = this->ttmessage.get_optimal_dim();
    this->ttmessage.set_wh(dim);
    this->ttmessage.set_xy(widget_rect.x + (widget_rect.cx - dim.w) / 2,
                           widget_rect.y + (widget_rect.cy - dim.h) / 2);
    this->ttmessage.rdp_input_invalidate(this->ttmessage.get_rect());
    this->set_mod_signal(BACK_EVENT_NONE);
}

TransitionMod::~TransitionMod() = default;

void TransitionMod::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    RailModBase::check_alt_f4(keymap);

    if (pressed_scancode(flags, scancode) == Scancode::Esc) {
        this->set_mod_signal(BACK_EVENT_STOP);
    }
    else {
        this->screen.rdp_input_scancode(flags, scancode, event_time, keymap);
    }
}
