/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "host/ui/export_dialog.h"

#include <imgui.h>

namespace oid::host {

bool draw_export_dialog(ExportDialogState& st) {
    if (!st.open) {
        return false;
    }

    // Calling OpenPopup() every frame while `st.open` stays true is the
    // documented pattern for a bool-flag-driven modal (ImGui de-dupes
    // consecutive-frame OpenPopup() calls against an already-open popup of
    // the same id -- see OpenPopupEx()'s OpenFrameCount check -- rather
    // than reopening it); the popup is actually raised the first frame
    // after open_export_dialog() flips `st.open` to true, since ImGui
    // processes an OpenPopup() + BeginPopupModal() pair in the same frame.
    //
    // The one case that check doesn't cover: ImGui dismissing the popup
    // itself earlier in this same frame (e.g. Escape closing the topmost
    // modal when nav is enabled -- NavUpdateCancelRequest runs during
    // ImGui::NewFrame(), before this draw call runs). The popup is then
    // absent from ImGui's open-popup stack, so the OpenPopup() call below
    // unconditionally re-raises it, and BeginPopupModal() below returns
    // true anyway -- the dialog becomes un-dismissable except via its own
    // Save/Cancel buttons. `st.open` is resynced from BeginPopupModal()'s
    // return value below (its `else` branch) to close that gap: on a
    // freshly-opened dialog BeginPopupModal() returns true this same
    // frame (per the pairing above), so that branch only fires when ImGui
    // genuinely dismissed an already-open popup on its own.
    ImGui::OpenPopup("Export buffer");

    bool saved = false;
    if (ImGui::BeginPopupModal(
            "Export buffer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(st.buffer_name.c_str());

        ImGui::InputText("##path", st.path_buf.data(), st.path_buf.size());
        if (ImGui::IsItemEdited()) {
            st.user_edited_path = true;
        }

        if (ImGui::RadioButton("PNG image (.png)",
                               st.format ==
                                   oid::BufferExporter::OutputType::BITMAP)) {
            st.format = oid::BufferExporter::OutputType::BITMAP;
            apply_format_extension(st);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(
                "Octave matrix (.oct)",
                st.format == oid::BufferExporter::OutputType::OCTAVE_MATRIX)) {
            st.format = oid::BufferExporter::OutputType::OCTAVE_MATRIX;
            apply_format_extension(st);
        }

        const bool path_empty = st.path_buf[0] == '\0';
        ImGui::BeginDisabled(path_empty);
        if (ImGui::Button("Save")) {
            saved = true;
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            st.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else if (st.open) {
        // See the comment above OpenPopup(): BeginPopupModal() returned
        // false even though we called OpenPopup() moments ago and `st.open`
        // is still true, meaning ImGui dismissed the popup on its own this
        // frame rather than through Save/Cancel. Resync so next frame's
        // OpenPopup() call is a genuine no-op instead of re-raising an
        // already-dismissed dialog.
        st.open = false;
    }

    return saved;
}

} // namespace oid::host
