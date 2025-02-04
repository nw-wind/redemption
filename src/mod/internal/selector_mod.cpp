/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2013
 *   Author(s): Christophe Grosjean, Xiaopeng Zhou, Jonathan Poelen, Meng Tan, Jennifer Inthavong
 */

#include "mod/internal/selector_mod.hpp"
#include "mod/internal/copy_paste.hpp"
#include "configs/config.hpp"
#include "core/font.hpp"
#include "gdi/osd_api.hpp"
#include "keyboard/keymap.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "utils/log.hpp"


namespace
{
    struct temporary_login
    {
        char buffer[256];

        explicit temporary_login(SelectorModVariables ini)
        {
            this->buffer[0] = 0;
            snprintf(
                this->buffer, sizeof(this->buffer),
                "%s@%s",
                ini.get<cfg::globals::auth_user>().c_str(),
                ini.get<cfg::globals::host>().c_str()
            );
        }
    };

    inline size_t proceed_item(const char * list)
    {
        const char * p = list;
        while (*p != '\x01' && *p != '\n' && *p) {
            p++;
        }
        return p - list;
    }

    constexpr int nb_max_row = 1024;
} // namespace


SelectorMod::SelectorMod(
    SelectorModVariables ini,
    gdi::GraphicApi & drawable,
    gdi::OsdApi& osd,
    FrontAPI & front, uint16_t width, uint16_t height,
    Rect const widget_rect, ClientExecute & rail_client_execute,
    Font const& font, Theme const& theme, CopyPaste& copy_paste
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, &copy_paste)
    , ini(ini)
    , osd(osd)
    , language_button(
        ini.get<cfg::internal_mod::keyboard_layout_proposals>(),
        this->selector, drawable, front, font, theme)

    , selector_params([&]() {
        WidgetSelectorParams params;

        params.nb_columns = 3;
        params.weight[0] = 20;
        params.weight[1] = 70;
        params.weight[2] = 10;

        Translator tr(ini.get<cfg::translation::language>());
        params.label[0] = tr(trkeys::authorization);
        params.label[1] = tr(trkeys::target);
        params.label[2] = tr(trkeys::protocol);

        return params;
    }())
    , selector(
        drawable, copy_paste, this->screen, temporary_login(ini).buffer,
        widget_rect.x, widget_rect.y, widget_rect.cx, widget_rect.cy,
        {
            .onconnect = [this]{
                char buffer[1024] = {};
                uint16_t row_index = 0;
                uint16_t column_index = 0;
                this->selector.selector_lines.get_selection(row_index, column_index);
                if (static_cast<uint16_t>(-1u) != row_index)
                {
                    const char * target = this->selector.selector_lines.get_cell_text(row_index, WidgetSelector::IDX_TARGET);
                    const char * groups = this->selector.selector_lines.get_cell_text(row_index, WidgetSelector::IDX_TARGETGROUP);
                    snprintf(buffer, sizeof(buffer), "%s:%s:%s",
                                target, groups, this->ini.get<cfg::globals::auth_user>().c_str());
                    this->ini.set_acl<cfg::globals::auth_user>(buffer);
                    this->ini.ask<cfg::globals::target_user>();
                    this->ini.ask<cfg::globals::target_device>();
                    this->ini.ask<cfg::context::target_protocol>();

                    this->set_mod_signal(BACK_EVENT_NEXT);
                    // throw Error(ERR_BACK_EVENT_NEXT);
                }
            },

            .oncancel = [this]{
                this->ini.ask<cfg::globals::auth_user>();
                this->ini.ask<cfg::context::password>();
                this->ini.set<cfg::context::selector>(false);
                this->set_mod_signal(BACK_EVENT_NEXT);
            },

            .onfilter = [this]{ this->ask_page(); },

            .onfirst_page = [this]{
                if (this->current_page > 1) {
                    this->current_page = 1;
                    this->ask_page();
                }
            },

            .onprev_page = [this]{
                if (this->current_page > 1) {
                    --this->current_page;
                    this->ask_page();
                }
            },

            .oncurrent_page = [this]{
                int page = unchecked_decimal_chars_to_int(this->selector.current_page.get_text());
                if (page != this->current_page) {
                    this->current_page = page;
                    this->ask_page();
                }
            },

            .onnext_page = [this]{
                if (this->current_page < this->number_page) {
                    ++this->current_page;
                    this->ask_page();
                }
            },

            .onlast_page = [this]{
                if (this->current_page < this->number_page) {
                    this->current_page = this->number_page;
                    this->ask_page();
                }
            },

            .onctrl_shift = [this]{ this->language_button.next_layout(); },
        },
        ini.is_asked<cfg::context::selector_current_page>()
            ? ""
            : int_to_decimal_zchars(ini.get<cfg::context::selector_current_page>()).c_str(),
        ini.is_asked<cfg::context::selector_number_of_pages>()
            ? ""
            : int_to_decimal_zchars(ini.get<cfg::context::selector_number_of_pages>()).c_str(),
        &this->language_button, this->selector_params, font, theme, language(ini), true)

    , current_page(unchecked_decimal_chars_to_int(this->selector.current_page.get_text()))
    , number_page(unchecked_decimal_chars_to_int(this->selector.number_page.get_text()+1))
{
    this->screen.add_widget(this->selector, WidgetComposite::HasFocus::Yes);
    this->screen.init_focus();

    uint16_t available_height = (this->selector.first_page.y() - 10) - this->selector.selector_lines.y();
    uint16_t line_height = font.max_height() + 2 * (
                            this->selector.selector_lines.border
                            +  this->selector.selector_lines.y_padding_label);

    this->selector_lines_per_page_saved = std::min<int>(available_height / line_height, nb_max_row);
    this->ini.set_acl<cfg::context::selector_lines_per_page>(this->selector_lines_per_page_saved);
    this->selector.rdp_input_invalidate(this->selector.get_rect());
    this->ask_page();
}

void SelectorMod::acl_update(AclFieldMask const& /*acl_fields*/)
{
    this->current_page = this->ini.get<cfg::context::selector_current_page>();
    this->selector.current_page.set_text(int_to_decimal_zchars(this->current_page).c_str());

    this->number_page = this->ini.get<cfg::context::selector_number_of_pages>();
    this->selector.number_page.set_text(WidgetSelector::temporary_number_of_page(
        int_to_decimal_zchars(this->number_page).c_str()).buffer);

    this->selector.selector_lines.clear();

    this->refresh_device();

    this->selector.rdp_input_invalidate(this->selector.get_rect());

    this->osd_banner_message();
}

void SelectorMod::ask_page()
{
    this->ini.set_acl<cfg::context::selector_current_page>(this->current_page);

    this->ini.set_acl<cfg::context::selector_group_filter>(this->selector.edit_filters[0].get_text());
    this->ini.set_acl<cfg::context::selector_device_filter>(this->selector.edit_filters[1].get_text());
    this->ini.set_acl<cfg::context::selector_proto_filter>(this->selector.edit_filters[2].get_text());

    this->ini.ask<cfg::globals::target_user>();
    this->ini.ask<cfg::globals::target_device>();
    this->ini.ask<cfg::context::selector>();
}

void SelectorMod::osd_banner_message()
{
    if (this->ini.get<cfg::context::banner_message>().empty())
    {
        return;
    }

    // Show OSD banner message only after primary auth

    gdi::OsdMsgUrgency omu = gdi::OsdMsgUrgency::NORMAL;

    switch (this->ini.get<cfg::context::banner_type>())
    {
        case BannerType::info :
            omu = gdi::OsdMsgUrgency::INFO;
            break;
        case BannerType::warn :
            omu = gdi::OsdMsgUrgency::WARNING;
            break;
        case BannerType::alert :
            omu = gdi::OsdMsgUrgency::ALERT;
            break;
    }

    this->osd.display_osd_message(
        this->ini.get<cfg::context::banner_message>(), omu);

    this->ini.set<cfg::context::banner_message>("");
}

void SelectorMod::refresh_device()
{
    char const* groups    = this->ini.get<cfg::globals::target_user>().c_str();
    char const* targets   = this->ini.get<cfg::globals::target_device>().c_str();
    char const* protocols = this->ini.get<cfg::context::target_protocol>().c_str();
    for (unsigned index = 0; index < this->ini.get<cfg::context::selector_lines_per_page>(); index++) {
        size_t size_groups = proceed_item(groups);
        if (!size_groups) {
            break;
        }
        size_t size_targets = proceed_item(targets);
        size_t size_protocols = proceed_item(protocols);

        chars_view const texts[] {
            {groups, size_groups},
            {targets, size_targets},
            {protocols, size_protocols},
        };
        this->selector.add_device(texts);

        if (groups[size_groups]       == '\n' || !groups[size_groups]
         || targets[size_targets]     == '\n' || !targets[size_targets]
         || protocols[size_protocols] == '\n' || !protocols[size_protocols]
        ){
            break;
        }

        groups += size_groups + 1;
        targets += size_targets + 1;
        protocols += size_protocols + 1;
    }

    if (this->selector.selector_lines.get_nb_rows() == 0) {
        this->selector.selector_lines.set_unfocusable();

        auto no_result = TR(trkeys::no_results, language(this->ini));
        chars_view const texts[] {{}, no_result, {}};
        this->selector.add_device(texts);
    }
    else {
        this->selector.selector_lines.set_focusable();

        this->selector.selector_lines.set_selection(0);
        this->selector.set_widget_focus(this->selector.selector_lines, Widget::focus_reason_tabkey);
    }

    this->selector.move_size_widget(
        this->selector.x(),
        this->selector.y(),
        this->selector.cx(),
        this->selector.cy());
}

void SelectorMod::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    RailModBase::check_alt_f4(keymap);

    if (&this->selector.selector_lines == this->selector.current_focus) {
        switch (underlying_cast(keymap.last_kevent()))
        {
        case underlying_cast(Keymap::KEvent::LeftArrow):
            if (this->current_page > 1) {
                --this->current_page;
                this->ask_page();
                return;
            }
            else if (this->current_page == 1 && this->number_page > 1) {
                this->current_page = this->number_page;
                this->ask_page();
                return;
            }
            return;

        case underlying_cast(Keymap::KEvent::RightArrow):
            if (this->current_page < this->number_page) {
                ++this->current_page;
                this->ask_page();
                return;
            }
            else if (this->current_page == this->number_page && this->number_page > 1) {
                this->current_page = 1;
                this->ask_page();
                return;
            }
            return;
        }
    }

    this->screen.rdp_input_scancode(flags, scancode, event_time, keymap);
}

void SelectorMod::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->selector.move_size_widget(left, top, width, height);

    uint16_t available_height = (this->selector.first_page.y() - 10) - this->selector.selector_lines.y();
    uint16_t line_height = this->screen.font.max_height() + 2 * (
                            this->selector.selector_lines.border
                            +  this->selector.selector_lines.y_padding_label);

    int const selector_lines_per_page = std::min<int>(available_height / line_height, nb_max_row);

    LOG(LOG_INFO, "selector lines per page = %d (%d)", selector_lines_per_page, this->selector_lines_per_page_saved);
    if (this->selector_lines_per_page_saved != selector_lines_per_page) {
        this->selector_lines_per_page_saved = selector_lines_per_page;
        this->ini.set_acl<cfg::context::selector_lines_per_page>(this->selector_lines_per_page_saved);
        this->selector.rdp_input_invalidate(this->selector.get_rect());
        this->ask_page();
    }
}
