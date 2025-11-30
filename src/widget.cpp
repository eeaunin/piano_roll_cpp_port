#include "piano_roll/widget.hpp"

#include "piano_roll/playback.hpp"

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

PianoRollWidget::PianoRollWidget()
    : PianoRollWidget(PianoRollConfig{}) {}

PianoRollWidget::PianoRollWidget(const PianoRollConfig& cfg)
    : coords_(cfg.piano_key_width),
      snap_(cfg.ticks_per_beat),
      renderer_(config_),
      pointer_(notes_, coords_, &snap_),
      keyboard_(notes_),
      loop_markers_(&coords_,
                    4 * cfg.ticks_per_beat,
                    8 * cfg.ticks_per_beat) {
    // Initial viewport: reasonable default.
    Viewport& vp = coords_.viewport();
    vp.width = 800.0;
    vp.height = 400.0;

    // Apply layout from config.
    top_padding_ = cfg.top_padding;
    ruler_height_ = cfg.ruler_height;
    footer_height_ = cfg.footer_height;
    note_label_width_ = cfg.note_label_width;

    // Musical defaults and clip bounds.
    coords_.set_ticks_per_beat(cfg.ticks_per_beat);
    renderer_.set_ticks_per_beat(cfg.ticks_per_beat);
    clip_start_tick_ = 0;
    clip_end_tick_ =
        static_cast<Tick>(cfg.default_clip_bars) *
        4 * static_cast<Tick>(cfg.ticks_per_beat);

    // Initialize explored area to the initial viewport.
    explored_min_x_ = vp.x;
    explored_max_x_ = vp.x + vp.width;

    loop_markers_.set_layout(top_padding_,
                             ruler_height_,
                             coords_.piano_key_width());

    // Default CC lanes: start with CC1 (mod wheel).
    cc_lanes_.push_back(ControlLane{1});
    active_cc_lane_ = 0;

    // Apply CC lane visibility/height from config.
    config_.show_cc_lane = cfg.show_cc_lane;
    config_.cc_lane_height = cfg.cc_lane_height;

    // Initialize scrollbar callbacks: keep viewport.x and explored area in sync.
    h_scrollbar_.on_scroll_update = [this](double new_scroll) {
        handle_scrollbar_scroll(new_scroll);
    };
    h_scrollbar_.on_edge_resize = [this](const char* edge, double delta_x) {
        handle_scrollbar_edge_resize(edge, delta_x);
    };
    h_scrollbar_.on_double_click = [this]() {
        handle_scrollbar_double_click();
    };
    h_scrollbar_.on_drag_end = [this]() {
        handle_scrollbar_drag_end();
    };

    keyboard_.set_snap_system(&snap_);
    keyboard_.set_coordinate_system(&coords_);
    pointer_.set_edge_threshold_pixels(10.0);
    pointer_.set_drag_threshold_pixels(4.0);
    pointer_.set_enable_ctrl_drag_duplicate(true);
}

void PianoRollWidget::set_playback_start_tick(
    Tick tick) noexcept {
    playback_start_tick_ = tick;
    show_playback_start_marker_ = true;
    if (on_playback_markers_changed_) {
        on_playback_markers_changed_(playback_start_tick_,
                                     cue_left_tick_,
                                     cue_right_tick_);
    }
}

void PianoRollWidget::set_cue_markers(Tick left,
                                      Tick right) noexcept {
    if (left <= right) {
        cue_left_tick_ = left;
        cue_right_tick_ = right;
    } else {
        cue_left_tick_ = right;
        cue_right_tick_ = left;
    }
    show_cue_markers_ = true;
    if (on_playback_markers_changed_) {
        on_playback_markers_changed_(playback_start_tick_,
                                     cue_left_tick_,
                                     cue_right_tick_);
    }
}

void PianoRollWidget::set_playhead(Tick tick) noexcept {
    if (tick < 0) {
        tick = 0;
    }
    renderer_.set_playhead(tick);
    if (on_playhead_changed_) {
        on_playhead_changed_(tick);
    }
}

void PianoRollWidget::set_ticks_per_beat(int ticks) noexcept {
    if (ticks <= 0) {
        return;
    }
    coords_.set_ticks_per_beat(ticks);
    snap_.set_ticks_per_beat(ticks);
    renderer_.set_ticks_per_beat(ticks);
    clip_end_tick_ =
        4 * 4 * static_cast<Tick>(ticks);
}

void PianoRollWidget::set_clip_bounds(Tick start,
                                      Tick end) noexcept {
    if (end < start) {
        std::swap(start, end);
    }
    Tick min_length =
        static_cast<Tick>(coords_.ticks_per_beat());
    if (end < start + min_length) {
        end = start + min_length;
    }
    clip_start_tick_ = start;
    clip_end_tick_ = end;
}

Tick PianoRollWidget::update_playback(Tick current_tick,
                                      double tempo_bpm,
                                      double delta_seconds) noexcept {
    const int tpb = coords_.ticks_per_beat();
    bool loop_on = loop_markers_.enabled;
    Tick loop_start = 0;
    Tick loop_end = 0;
    if (loop_on) {
        auto range = loop_markers_.tick_range();
        loop_start = range.first;
        loop_end = range.second;
        if (loop_end <= loop_start) {
            loop_on = false;
        }
    }

    Tick new_tick = advance_playback_ticks(current_tick,
                                           tempo_bpm,
                                           tpb,
                                           delta_seconds,
                                           loop_on,
                                           loop_start,
                                           loop_end);
    set_playhead(new_tick);
    return playhead_tick();
}

bool PianoRollWidget::hovered_note(NoteId& id_out,
                                   HoverEdge& edge_out) const noexcept {
    HoverState hs = pointer_.hover_state();
    if (!hs.has_note) {
        return false;
    }
    id_out = hs.note_id;
    edge_out = hs.edge;
    return true;
}

bool PianoRollWidget::hovered_note_world(double& x1,
                                         double& y1,
                                         double& x2,
                                         double& y2,
                                         HoverEdge& edge_out) const noexcept {
    return pointer_.hovered_note_world(x1, y1, x2, y2, edge_out);
}

void PianoRollWidget::fit_view_to_clip() noexcept {
    handle_scrollbar_double_click();
}

void PianoRollWidget::fit_view_to_selection() noexcept {
    Tick min_tick{};
    Tick max_tick{};
    MidiKey min_key{};
    MidiKey max_key{};
    if (!selection_bounds(min_tick, max_tick, min_key, max_key)) {
        return;
    }

    // Compute world-space bounds for the selection.
    double min_x = coords_.tick_to_world(min_tick);
    double max_x = coords_.tick_to_world(max_tick);
    double top_y = coords_.key_to_world_y(max_key);
    double bottom_y =
        coords_.key_to_world_y(min_key) + coords_.key_height();

    Viewport& vp = coords_.viewport();
    double view_width = vp.width;
    double view_height = vp.height;

    if (view_width <= 0.0 || view_height <= 0.0) {
        return;
    }

    // Add a small padding around the selection, similar in spirit to the
    // Python _ensure_selected_notes_visible behaviour.
    const double horizontal_padding =
        view_width * 0.05;  // 5% of view width
    const double vertical_padding =
        view_height * 0.05;  // 5% of view height

    double target_left = min_x - horizontal_padding;
    double target_right = max_x + horizontal_padding;
    if (target_right <= target_left) {
        target_right = target_left + 1.0;
    }

    double selection_height = bottom_y - top_y;
    if (selection_height <= 0.0) {
        selection_height = coords_.key_height();
    }

    // Adjust horizontal zoom so the selection fits within the viewport.
    double required_ppb =
        view_width /
        std::max(1.0, (target_right - target_left));
    coords_.set_pixels_per_beat(required_ppb);

    // Recompute world-space bounds with the new zoom.
    target_left = coords_.tick_to_world(min_tick) - horizontal_padding;
    target_right = coords_.tick_to_world(max_tick) + horizontal_padding;
    if (target_right <= target_left) {
        target_right = target_left + 1.0;
    }

    double new_view_x =
        target_left;

    // Vertical centering: keep selection roughly centered while respecting
    // vertical scroll clamping.
    double selection_center_y =
        top_y + selection_height * 0.5;
    double new_view_y =
        selection_center_y - view_height * 0.5;

    coords_.set_scroll(new_view_x, new_view_y);
    expand_explored_area(new_view_x);
    update_scrollbar_geometry();
}

bool PianoRollWidget::selection_bounds(Tick& min_tick,
                                       Tick& max_tick,
                                       MidiKey& min_key,
                                       MidiKey& max_key) const noexcept {
    const auto& ns = notes_.notes();
    bool any_selected = false;
    for (const Note& n : ns) {
        if (!n.selected) {
            continue;
        }
        if (!any_selected) {
            any_selected = true;
            min_tick = n.tick;
            max_tick = n.end_tick();
            min_key = n.key;
            max_key = n.key;
        } else {
            if (n.tick < min_tick) min_tick = n.tick;
            if (n.end_tick() > max_tick) max_tick = n.end_tick();
            if (n.key < min_key) min_key = n.key;
            if (n.key > max_key) max_key = n.key;
        }
    }
    return any_selected;
}

void PianoRollWidget::draw() {
#ifdef PIANO_ROLL_USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    // Update piano-key flash timer (short visual highlight after presses).
    if (piano_key_flash_timer_ > 0.0f) {
        piano_key_flash_timer_ -= io.DeltaTime;
        if (piano_key_flash_timer_ <= 0.0f) {
            piano_key_flash_timer_ = 0.0f;
            has_pressed_piano_key_ = false;
        }
    }

    // Fit viewport to available content area.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f || avail.y <= 0.0f) {
        return;
    }

    Viewport& vp = coords_.viewport();
    vp.width =
        static_cast<double>(avail.x) - coords_.piano_key_width();
    if (vp.width < 100.0) {
        vp.width = 100.0;
    }
    vp.height = static_cast<double>(avail.y);

    // Ensure the explored area covers all notes so the scrollbar reflects
    // newly added content (mirrors Python _update_explored_area_for_notes).
    update_explored_area_for_notes();

    // Update scrollbar geometry before rendering (uses current viewport).
    update_scrollbar_geometry();

    // Zoom control (px/beat) – kept for now; will converge to scrollbar-based
    // zoom once all behaviours are ported.
    {
        double ppb = coords_.pixels_per_beat();
        float zoom = static_cast<float>(ppb);
        if (ImGui::SliderFloat("Zoom (px/beat)",
                               &zoom,
                               15.0f,
                               4000.0f)) {
            coords_.set_zoom(static_cast<double>(zoom));
        }
    }

    // Snap settings (mode + division), mirroring the Python snap menu.
    {
        int mode_index = 0;
        switch (snap_.snap_mode()) {
        case SnapMode::Off:
            mode_index = 0;
            break;
        case SnapMode::Adaptive:
            mode_index = 1;
            break;
        case SnapMode::Manual:
            mode_index = 2;
            break;
        }
        const char* mode_labels[] = {
            "Snap Off",
            "Snap Adaptive",
            "Snap Manual",
        };
        if (ImGui::Combo("Snap Mode",
                         &mode_index,
                         mode_labels,
                         IM_ARRAYSIZE(mode_labels))) {
            SnapMode new_mode = SnapMode::Adaptive;
            if (mode_index == 0) {
                new_mode = SnapMode::Off;
            } else if (mode_index == 1) {
                new_mode = SnapMode::Adaptive;
            } else {
                new_mode = SnapMode::Manual;
            }
            snap_.set_snap_mode(new_mode);
        }

        static const char* division_labels[] = {
            "1/64",
            "1/32",
            "1/16",
            "1/8",
            "1/4",
            "1/2",
            "1 bar",
            "2 bars",
            "4 bars",
        };
        int current_div_index = 0;
        const std::string& current_label =
            snap_.snap_division().label;
        for (int i = 0;
             i < static_cast<int>(IM_ARRAYSIZE(division_labels));
             ++i) {
            if (current_label == division_labels[i]) {
                current_div_index = i;
                break;
            }
        }
        if (ImGui::Combo("Snap Division",
                         &current_div_index,
                         division_labels,
                         IM_ARRAYSIZE(division_labels))) {
            const char* chosen =
                division_labels[current_div_index];
            snap_.set_snap_division(chosen);
        }

        // Display human-readable snap info (e.g. "Snap: ADAPTIVE (1/16)")
        // similar to the Python status text.
        ImGui::TextUnformatted(snap_.snap_info().c_str());
    }

    // CC lane selector (if any lanes are present).
    if (!cc_lanes_.empty()) {
        const char* combo_label = "CC Lane";
        std::string preview;
        if (config_.show_cc_lane && active_cc_lane_ >= 0 &&
            active_cc_lane_ < static_cast<int>(cc_lanes_.size())) {
            preview = "CC " + std::to_string(
                                   cc_lanes_[static_cast<std::size_t>(
                                       active_cc_lane_)]
                                       .cc_number());
        } else {
            preview = "None";
        }

        if (ImGui::BeginCombo(combo_label, preview.c_str())) {
            // "None" option hides the CC lane.
            bool selected_none = !config_.show_cc_lane;
            if (ImGui::Selectable("None", selected_none)) {
                config_.show_cc_lane = false;
            }

            for (std::size_t i = 0; i < cc_lanes_.size(); ++i) {
                const int cc = cc_lanes_[i].cc_number();
                std::string label = "CC " + std::to_string(cc);
                bool is_selected =
                    config_.show_cc_lane &&
                    static_cast<int>(i) == active_cc_lane_;
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    active_cc_lane_ = static_cast<int>(i);
                    config_.show_cc_lane = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    // Render the piano roll.
    renderer_.render(coords_, notes_);

    // Auto-scroll to keep playhead near edges when enabled.
    if (config_.playhead_auto_scroll && renderer_.has_playhead()) {
        double ppb = coords_.pixels_per_beat();
        if (ppb > 0.0) {
            double playhead_x =
                static_cast<double>(renderer_.playhead_tick()) /
                static_cast<double>(coords_.ticks_per_beat()) *
                ppb;
            Viewport& vp_auto = coords_.viewport();
            double viewport_end = vp_auto.x + vp_auto.width;
            double margin =
                static_cast<double>(config_.playhead_auto_scroll_margin);

            if (playhead_x < vp_auto.x + margin) {
                double new_x = playhead_x - margin;
                coords_.set_scroll(new_x, vp_auto.y);
                expand_explored_area(new_x);
                update_scrollbar_geometry();
            } else if (playhead_x > viewport_end - margin) {
                double new_x =
                    playhead_x - vp_auto.width + margin;
                coords_.set_scroll(new_x, vp_auto.y);
                expand_explored_area(new_x);
                update_scrollbar_geometry();
            }
        }
    }

    // Piano-key note labels in the left label column (C4, D#3, etc.).
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_min = ImGui::GetItemRectMin();
        ImVec2 canvas_max = ImGui::GetItemRectMax();

        float view_top =
            canvas_min.y + top_padding_ + ruler_height_;
        float view_bottom = canvas_max.y;

        auto to_imvec4 = [](const ColorRGBA& c) {
            return ImVec4(c.r, c.g, c.b, c.a);
        };
        auto to_color = [&](const ColorRGBA& c) {
            return ImGui::ColorConvertFloat4ToU32(
                to_imvec4(c));
        };

        const auto key_range = coords_.visible_key_range();
        MidiKey min_key = key_range.first;
        MidiKey max_key = key_range.second;

        const char* names[12] = {
            "C",  "C#",
            "D",  "D#",
            "E",  "F",
            "F#", "G",
            "G#", "A",
            "A#", "B"};

        double ppk = coords_.key_height();
        const double min_all_labels = 20.0;
        const double min_some_labels = 12.0;

        ImFont* font = ImGui::GetFont();
        float font_size = font ? font->FontSize
                               : ImGui::GetFontSize();

        for (MidiKey key = min_key; key <= max_key; ++key) {
            double world_y = coords_.key_to_world_y(key);
            double world_y_next =
                world_y + coords_.key_height();

            auto [sx1_local, sy1_local] =
                coords_.world_to_screen(0.0, world_y);
            auto [sx2_local, sy2_local] =
                coords_.world_to_screen(0.0, world_y_next);

            float y1 = canvas_min.y +
                       static_cast<float>(sy1_local);
            float y2 = canvas_min.y +
                       static_cast<float>(sy2_local);

            if (y2 < view_top || y1 > view_bottom) {
                continue;
            }

            float y1_draw = std::max(y1, view_top);
            float y2_draw = std::min(y2, view_bottom);

            int note_index = key % 12;
            const char* name = names[note_index];
            int octave = key / 12 - 2;
            std::string label = name;
            label += std::to_string(octave);

            bool show = false;
            if (ppk >= min_all_labels) {
                show = true;
            } else if (ppk >= min_some_labels) {
                show = (note_index == 0 || note_index == 5);
            } else {
                show = (note_index == 0);
            }

            if (show) {
                ImVec2 size =
                    ImGui::CalcTextSize(label.c_str());
                float text_y =
                    y1_draw + (y2_draw - y1_draw - size.y) *
                                  0.5f;
                if (text_y + size.y > view_bottom) {
                    continue;
                }

                float padding = 10.0f;
                float text_x =
                    canvas_min.x + note_label_width_ -
                    padding - size.x;

                draw_list->AddText(
                    ImVec2(text_x, text_y),
                    to_color(config_.note_label_text_color),
                    label.c_str());

                if (note_index == 0) {
                    float line_y = y2_draw - 0.5f;
                    float line_start_x =
                        std::max(canvas_min.x,
                                 text_x - 20.0f);
                    draw_list->AddLine(
                        ImVec2(line_start_x, line_y),
                        ImVec2(canvas_min.x +
                                   note_label_width_,
                               line_y),
                        to_color(config_.grid_line_color),
                        1.0f);
                }
            }
        }
    }

    // Loop region in the ruler (Bitwig-style loop markers).
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_min = ImGui::GetItemRectMin();
        loop_markers_.set_layout(top_padding_,
                                 ruler_height_,
                                 coords_.piano_key_width());
        loop_markers_.update_bounds_from_ticks();
        loop_markers_.render(draw_list,
                             config_,
                             canvas_min.x,
                             canvas_min.y);
    }

    // Ruler, playback markers, and piano-key area visual feedback overlays.
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_min = ImGui::GetItemRectMin();
        ImVec2 canvas_max = ImGui::GetItemRectMax();
        float piano_key_width =
            static_cast<float>(coords_.piano_key_width());

        auto to_imvec4 = [](const ColorRGBA& c) {
            return ImVec4(c.r, c.g, c.b, c.a);
        };
        auto to_color = [&](const ColorRGBA& c) {
            return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
        };

        // Playback start marker in top ruler section (triangle plus faint line).
        if (show_playback_start_marker_) {
            double world_x =
                coords_.tick_to_world(playback_start_tick_);
            auto [sx_local, sy_local] =
                coords_.world_to_screen(world_x, 0.0);
            (void)sy_local;
            float x = canvas_min.x +
                      static_cast<float>(sx_local);
            float grid_left =
                canvas_min.x + piano_key_width;
            float grid_right = canvas_max.x;
            if (x >= grid_left && x <= grid_right) {
                float marker_y =
                    canvas_min.y + top_padding_ + 8.0f;
                float size = 10.0f;
                ImVec2 p0(x,
                          marker_y - size * 1.5f);
                ImVec2 p1(x,
                          marker_y - size * 0.5f);
                ImVec2 p2(x + size * 0.866f,
                          marker_y - size);
                ImU32 col = to_color(
                    config_.playback_start_marker_color);
                draw_list->AddTriangleFilled(
                    p0, p1, p2, col);
                draw_list->AddLine(
                    ImVec2(x,
                           canvas_min.y + top_padding_),
                    ImVec2(x,
                           canvas_min.y + top_padding_ +
                               ruler_height_),
                    col,
                    1.0f);
            }
        }

        // Cue markers in bottom ruler section (left/right triangles).
        if (show_cue_markers_ &&
            cue_right_tick_ > cue_left_tick_) {
            float bottom_section_top =
                canvas_min.y + top_padding_ +
                ruler_height_ * 0.65f;
            float marker_y =
                bottom_section_top + 8.0f;
            float marker_size = 14.0f;

            ImU32 cue_col =
                to_color(config_.cue_marker_color);

            auto draw_cue = [&](Tick tick,
                                bool left_marker) {
                double world_x =
                    coords_.tick_to_world(tick);
                auto [sx_local, sy_local] =
                    coords_.world_to_screen(
                        world_x, 0.0);
                (void)sy_local;
                float x = canvas_min.x +
                          static_cast<float>(
                              sx_local);
                float grid_left =
                    canvas_min.x + piano_key_width;
                float grid_right = canvas_max.x;
                if (x < grid_left || x > grid_right) {
                    return;
                }
                if (left_marker) {
                    ImVec2 p0(x,
                              marker_y -
                                  marker_size * 0.5f);
                    ImVec2 p1(x,
                              marker_y +
                                  marker_size * 0.5f);
                    ImVec2 p2(x + marker_size,
                              marker_y);
                    draw_list->AddTriangleFilled(
                        p0, p1, p2, cue_col);
                } else {
                    ImVec2 p0(x,
                              marker_y -
                                  marker_size * 0.5f);
                    ImVec2 p1(x,
                              marker_y +
                                  marker_size * 0.5f);
                    ImVec2 p2(x - marker_size,
                              marker_y);
                    draw_list->AddTriangleFilled(
                        p0, p1, p2, cue_col);
                }
            };

            draw_cue(cue_left_tick_, true);
            draw_cue(cue_right_tick_, false);
        }

        // Ruler highlight when interacting.
        if (ruler_interaction_active_) {
            ColorRGBA c = config_.ruler_background_color;
            c.a = std::min(1.0f, c.a + 0.2f);
            ImVec2 rmin(canvas_min.x + piano_key_width,
                        canvas_min.y + top_padding_);
            ImVec2 rmax(canvas_min.x + piano_key_width +
                            static_cast<float>(coords_.viewport().width),
                        canvas_min.y + top_padding_ + ruler_height_);
            draw_list->AddRectFilled(rmin, rmax, to_color(c));
        }

        // Piano-key column darkening when note-name interaction is active.
        if (note_names_interaction_active_) {
            ColorRGBA c = config_.white_key_color;
            c.r *= 0.8f;
            c.g *= 0.8f;
            c.b *= 0.8f;
            ImVec2 kmin(canvas_min.x,
                        canvas_min.y + top_padding_ + ruler_height_);
            ImVec2 kmax(canvas_min.x + piano_key_width,
                        canvas_max.y);
            draw_list->AddRectFilled(kmin, kmax, to_color(c));
        }

        // Piano-key hover / press highlight over the key strip.
        if ((has_hovered_piano_key_ || has_pressed_piano_key_)) {
            MidiKey key =
                has_pressed_piano_key_
                    ? pressed_piano_key_
                    : hovered_piano_key_;
            double world_y =
                coords_.key_to_world_y(key);
            double world_y2 =
                world_y + coords_.key_height();
            auto [sx1_local, sy1_local] =
                coords_.world_to_screen(0.0, world_y);
            auto [sx2_local, sy2_local] =
                coords_.world_to_screen(0.0, world_y2);
            float y1 = canvas_min.y +
                       static_cast<float>(sy1_local);
            float y2 = canvas_min.y +
                       static_cast<float>(sy2_local);
            float x1 = canvas_min.x;
            float x2 = canvas_min.x + piano_key_width;
            ColorRGBA c =
                has_pressed_piano_key_
                    ? ColorRGBA{0.39f, 0.59f, 1.0f, 1.0f}
                    : ColorRGBA{0.78f, 0.86f, 1.0f, 1.0f};
            draw_list->AddRectFilled(ImVec2(x1, y1),
                                     ImVec2(x2, y2),
                                     to_color(c));
        }

        // MIDI clip boundary brackets in the ruler (Bitwig-style).
        if (clip_end_tick_ > clip_start_tick_) {
            auto draw_bracket = [&](Tick tick, bool is_start) {
                double world_x = coords_.tick_to_world(tick);
                auto [sx_local, sy_local] =
                    coords_.world_to_screen(world_x, 0.0);
                (void)sy_local;
                float x = canvas_min.x +
                          static_cast<float>(sx_local);
                float grid_left =
                    canvas_min.x + piano_key_width;
                float grid_right = canvas_max.x;
                if (x < grid_left || x > grid_right) {
                    return;
                }
                ImVec2 p0(x, canvas_min.y + top_padding_);
                ImVec2 p1(x,
                          canvas_min.y + top_padding_ + 8.0f);
                ImVec2 p2_start =
                    is_start ? ImVec2(x, canvas_min.y + top_padding_)
                             : ImVec2(x - 5.0f,
                                      canvas_min.y + top_padding_);
                ImVec2 p2_end =
                    is_start ? ImVec2(x + 5.0f,
                                      canvas_min.y + top_padding_)
                             : ImVec2(x,
                                      canvas_min.y + top_padding_);
                ColorRGBA c = config_.ruler_clip_boundary_color;
                ImU32 col = to_color(c);
                draw_list->AddLine(p0, p1, col, 2.0f);
                draw_list->AddLine(p2_start, p2_end, col, 2.0f);
            };

            draw_bracket(clip_start_tick_, true);
            draw_bracket(clip_end_tick_, false);
        }
    }

    // Render scrollbar on top.
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        h_scrollbar_.render(draw_list);
    }

    // Pointer, CC lane pointer, and keyboard interactions.
    handle_pointer_events();
    handle_keyboard_events();

    // Selection overlay and CC lane.
    RenderSelectionOverlay(notes_, pointer_, coords_, config_);
    if (config_.show_cc_lane && active_cc_lane_ >= 0 &&
        active_cc_lane_ < static_cast<int>(cc_lanes_.size())) {
        RenderControlLane(
            cc_lanes_[static_cast<std::size_t>(active_cc_lane_)],
            coords_,
            config_);
    }

    // Debug clicked-cell highlight (uses same coordinate math as Python's
    // last_clicked_cell overlay).
    if (has_last_clicked_cell_) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_min = ImGui::GetItemRectMin();
        ImVec2 canvas_max = ImGui::GetItemRectMax();

        double world_x1 =
            coords_.tick_to_world(last_clicked_tick_start_);
        double world_x2 =
            coords_.tick_to_world(last_clicked_tick_end_);
        double world_y1 =
            coords_.key_to_world_y(last_clicked_key_);
        double world_y2 =
            world_y1 + coords_.key_height();

        auto [sx1_local, sy1_local] =
            coords_.world_to_screen(world_x1, world_y1);
        auto [sx2_local, sy2_local] =
            coords_.world_to_screen(world_x2, world_y2);

        float x1 = canvas_min.x +
                   static_cast<float>(sx1_local);
        float x2 = canvas_min.x +
                   static_cast<float>(sx2_local);
        float y1 = canvas_min.y +
                   static_cast<float>(sy1_local);
        float y2 = canvas_min.y +
                   static_cast<float>(sy2_local);

        float grid_left =
            canvas_min.x +
            static_cast<float>(coords_.piano_key_width());
        float grid_top =
            canvas_min.y + top_padding_ + ruler_height_;
        float grid_right = canvas_max.x;
        float grid_bottom = canvas_max.y;

        if (x1 < grid_left) x1 = grid_left;
        if (x2 > grid_right) x2 = grid_right;
        if (y1 < grid_top) y1 = grid_top;
        if (y2 > grid_bottom) y2 = grid_bottom;

        if (x2 > x1 && y2 > y1) {
            auto to_imvec4 = [](const ColorRGBA& c) {
                return ImVec4(c.r, c.g, c.b, c.a);
            };
            auto to_color = [&](const ColorRGBA& c) {
                return ImGui::ColorConvertFloat4ToU32(
                    to_imvec4(c));
            };
            ColorRGBA fill{1.0f, 1.0f, 1.0f, 0.20f};
            draw_list->AddRectFilled(ImVec2(x1, y1),
                                     ImVec2(x2, y2),
                                     to_color(fill));
        }
    }

    // Debug cursor line overlay on top of everything if enabled.
    if (show_debug_crosshair_ &&
        debug_mouse_x_local_ >= 0.0f &&
        debug_mouse_y_local_ >= 0.0f) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_min = ImGui::GetItemRectMin();
        ImVec2 canvas_max = ImGui::GetItemRectMax();
        float x = canvas_min.x + debug_mouse_x_local_;
        ImU32 col = ImGui::GetColorU32(
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        draw_list->AddLine(ImVec2(x, canvas_min.y),
                           ImVec2(x, canvas_max.y),
                           col,
                           1.0f);
    }
#else
    // No Dear ImGui; nothing to draw.
#endif
}

void PianoRollWidget::handle_pointer_events() {
#ifdef PIANO_ROLL_USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 canvas_min = ImGui::GetItemRectMin();
    ImVec2 canvas_max = ImGui::GetItemRectMax();
    ImVec2 mouse = io.MousePos;

    if (mouse.x < canvas_min.x || mouse.x > canvas_max.x ||
        mouse.y < canvas_min.y || mouse.y > canvas_max.y) {
        return;
    }

    float local_x = mouse.x - canvas_min.x;
    float local_y = mouse.y - canvas_min.y;

    float total_height = canvas_max.y - canvas_min.y;
    float lane_height = config_.cc_lane_height;
    if (lane_height <= 0.0f || lane_height > total_height * 0.8f) {
        lane_height = total_height * 0.25f;
    }
    float lane_top_local = total_height - lane_height;
    float lane_bottom_local = total_height;

    ModifierKeys mods{
        .shift = io.KeyShift,
        .ctrl = io.KeyCtrl,
        .alt = io.KeyAlt,
    };

    const bool in_cc_lane =
        config_.show_cc_lane &&
        local_y >= lane_top_local && local_y <= lane_bottom_local;

    // Mouse wheel: vertical scroll only (Bitwig-style).
    float wheel = io.MouseWheel;
    if (wheel != 0.0f) {
        constexpr float scroll_speed = 30.0f;  // pixels per wheel notch
        double new_y =
            coords_.viewport().y -
            static_cast<double>(wheel) * scroll_speed;
        coords_.set_scroll(coords_.viewport().x, new_y);
    }

    // Record mouse position for debug crosshair.
    debug_mouse_x_local_ = local_x;
    debug_mouse_y_local_ = local_y;

    // Ruler and note-name area interactions (partial port).
    bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    float piano_key_width = static_cast<float>(coords_.piano_key_width());
    bool in_ruler =
        local_x >= piano_key_width &&
        local_y >= top_padding_ &&
        local_y <= top_padding_ + ruler_height_;

    // If a playback marker drag is active, update it first and short-circuit
    // other interactions. This mirrors the Python ordering where marker drags
    // take precedence over ruler pan/zoom.
    bool playback_marker_active =
        dragging_playback_start_ || dragging_cue_left_ ||
        dragging_cue_right_;
    if (playback_marker_active) {
        if (left_down) {
            auto [world_x, /*world_y*/ _] =
                coords_.screen_to_world(local_x, 0.0);
            Tick tick_raw = coords_.world_to_tick(world_x);
            Tick tick = tick_raw;
            if (!mods.shift) {
                tick = snap_.snap_tick(tick_raw);
            }

            if (dragging_playback_start_) {
                playback_start_tick_ =
                    std::max<Tick>(0, tick);
            } else if (dragging_cue_left_) {
                cue_left_tick_ = tick;
                if (cue_right_tick_ < cue_left_tick_) {
                    cue_right_tick_ = cue_left_tick_;
                }
            } else if (dragging_cue_right_) {
                cue_right_tick_ = tick;
                if (cue_right_tick_ < cue_left_tick_) {
                    cue_left_tick_ = cue_right_tick_;
                }
            }
        }
        if (left_released) {
            dragging_playback_start_ = false;
            dragging_cue_left_ = false;
            dragging_cue_right_ = false;
            if (on_playback_markers_changed_) {
                on_playback_markers_changed_(playback_start_tick_,
                                             cue_left_tick_,
                                             cue_right_tick_);
            }
        }
        return;
    }

    // Loop marker hover state when in ruler area.
    if (in_ruler && loop_markers_.enabled &&
        loop_markers_.visible) {
        loop_markers_.handle_mouse_move(local_x, local_y);
    } else if (loop_markers_.interaction_state ==
                   InteractionState::HoveringBody ||
               loop_markers_.interaction_state ==
                   InteractionState::HoveringLeftEdge ||
               loop_markers_.interaction_state ==
                   InteractionState::HoveringRightEdge) {
        loop_markers_.interaction_state =
            InteractionState::Idle;
    }

    // If loop marker is being dragged or resized, let it consume the drag and
    // mouse-up events, mirroring the Python interaction ordering.
    bool loop_active =
        loop_markers_.interaction_state ==
            InteractionState::Dragging ||
        loop_markers_.interaction_state ==
            InteractionState::ResizingLeft ||
        loop_markers_.interaction_state ==
            InteractionState::ResizingRight;
    if (loop_active) {
        if (left_down) {
            loop_markers_.handle_mouse_drag(local_x,
                                            local_y);
        }
        if (left_released) {
            loop_markers_.handle_mouse_up(local_x,
                                          local_y,
                                          0);
        }
        return;
    }

    // Start ruler interaction on click in ruler area, but give playback
    // markers and loop markers first chance to capture the press.
    if (left_clicked && in_ruler) {
        // Hit-test playback start and cue markers in screen space using a
        // horizontal tolerance.
        const float hit_px = 8.0f;

        auto marker_local_x = [&](Tick tick) -> std::optional<float> {
            double world_x = coords_.tick_to_world(tick);
            auto [sx_local, sy_local] =
                coords_.world_to_screen(world_x, 0.0);
            (void)sy_local;
            float x = static_cast<float>(sx_local);
            float grid_left = piano_key_width;
            float grid_right =
                static_cast<float>(coords_.piano_key_width() +
                                   coords_.viewport().width);
            if (x < grid_left || x > grid_right) {
                return std::nullopt;
            }
            return x;
        };

        bool handled_marker = false;

        if (show_playback_start_marker_) {
            if (auto x = marker_local_x(playback_start_tick_)) {
                if (std::abs(local_x - *x) <= hit_px) {
                    dragging_playback_start_ = true;
                    handled_marker = true;
                }
            }
        }

        if (!handled_marker && show_cue_markers_) {
            if (auto x_left = marker_local_x(cue_left_tick_)) {
                if (std::abs(local_x - *x_left) <= hit_px) {
                    dragging_cue_left_ = true;
                    handled_marker = true;
                }
            }
            if (!handled_marker) {
                if (auto x_right =
                        marker_local_x(cue_right_tick_)) {
                    if (std::abs(local_x - *x_right) <= hit_px) {
                        dragging_cue_right_ = true;
                        handled_marker = true;
                    }
                }
            }
        }

        if (!handled_marker &&
            !loop_markers_.handle_mouse_down(local_x,
                                             local_y,
                                             0)) {
            ruler_interaction_active_ = true;
            ruler_pan_active_ = false;
            horizontal_zoom_active_ = false;
            ruler_start_x_ = local_x;
            ruler_start_y_ = local_y;
            ruler_start_viewport_x_ = coords_.viewport().x;
            horizontal_zoom_start_pixels_per_beat_ =
                coords_.pixels_per_beat();
            ruler_initial_mouse_x_ = local_x;
            ruler_initial_mouse_y_ = local_y;
        }
    }

    // Start note-names interaction (vertical pan/zoom) when clicking in label area.
    if (left_clicked &&
        local_x >= 0.0f && local_x <= note_label_width_ &&
        local_y >= top_padding_ + ruler_height_) {
        note_names_interaction_active_ = true;
        note_names_pan_active_ = false;
        vertical_zoom_active_ = false;
        note_names_start_x_ = local_x;
        note_names_start_y_ = local_y;
        note_names_start_viewport_y_ = coords_.viewport().y;
        vertical_zoom_start_pixels_per_key_ = coords_.key_height();
        note_names_initial_mouse_x_ = local_x;
        note_names_initial_mouse_y_ = local_y;
        vertical_zoom_anchor_y_ = local_y;
    }

    // Handle active ruler interaction: decide pan vs zoom, then apply.
    if (ruler_interaction_active_) {
        if (left_down) {
            // Decide gesture type once movement exceeds threshold.
            if (!ruler_pan_active_ && !horizontal_zoom_active_) {
                double dx = std::abs(local_x - ruler_initial_mouse_x_);
                double dy = std::abs(local_y - ruler_initial_mouse_y_);
                if (dx > 3.0 || dy > 3.0) {
                    if (dx > dy * 1.5) {
                        ruler_pan_active_ = true;
                    } else {
                        horizontal_zoom_active_ = true;
                        horizontal_zoom_anchor_x_ = ruler_initial_mouse_x_;
                        horizontal_zoom_start_y_ = ruler_initial_mouse_y_;

                        double view_width = coords_.viewport().width;
                        double anchor_fraction =
                            (ruler_initial_mouse_x_ - piano_key_width) /
                            std::max(1.0, view_width);
                        double viewport_x = coords_.viewport().x;
                        double left_beat =
                            viewport_x / horizontal_zoom_start_pixels_per_beat_;
                        double visible_beats =
                            view_width / horizontal_zoom_start_pixels_per_beat_;
                        horizontal_zoom_anchor_beat_ =
                            left_beat + anchor_fraction * visible_beats;
                    }
                }
            }

            if (ruler_pan_active_) {
                // Drag left moves view right.
                double delta_x = -(local_x - ruler_start_x_);
                double new_viewport_x =
                    ruler_start_viewport_x_ + delta_x;
                coords_.set_scroll(new_viewport_x,
                                   coords_.viewport().y);
                expand_explored_area(new_viewport_x);
                ruler_start_x_ = local_x;
                ruler_start_viewport_x_ = coords_.viewport().x;
            } else if (horizontal_zoom_active_) {
                // Horizontal zoom uses vertical movement.
                double delta_y = local_y - horizontal_zoom_start_y_;
                double zoom_sensitivity = 0.01;
                double zoom_factor =
                    1.0 + (delta_y * zoom_sensitivity);
                double new_ppb =
                    horizontal_zoom_start_pixels_per_beat_ * zoom_factor;
                new_ppb = std::max(15.0, std::min(4000.0, new_ppb));

                double old_ppb = coords_.pixels_per_beat();
                double old_viewport_x = coords_.viewport().x;
                double mouse_x_in_view =
                    horizontal_zoom_anchor_x_ - piano_key_width;
                double beats_under_mouse =
                    (old_viewport_x + mouse_x_in_view) / old_ppb;

                coords_.set_pixels_per_beat(new_ppb);
                double new_viewport_x =
                    beats_under_mouse * new_ppb - mouse_x_in_view;
                coords_.set_scroll(new_viewport_x,
                                   coords_.viewport().y);
                expand_explored_area(new_viewport_x);
            }
        }
    }

    // Stop ruler interaction on release.
    if (left_released && ruler_interaction_active_) {
        // If no pan/zoom gesture was recognized, treat this as a simple
        // click in the ruler area and update the playhead position, matching
        // the Python playback handle_click behaviour (with playback/loop
        // markers already having taken precedence earlier).
        if (!ruler_pan_active_ && !horizontal_zoom_active_ && in_ruler) {
            auto [world_x, /*world_y*/ _] =
                coords_.screen_to_world(local_x, 0.0);
            Tick tick = coords_.world_to_tick(world_x);
            set_playhead(tick);
        }

        ruler_interaction_active_ = false;
        ruler_pan_active_ = false;
        horizontal_zoom_active_ = false;
    }

    // Note-names interaction: pan/zoom in vertical direction.
    if (note_names_interaction_active_) {
        if (left_down) {
            if (!note_names_pan_active_ && !vertical_zoom_active_) {
                double dx = std::abs(local_x - note_names_initial_mouse_x_);
                double dy = std::abs(local_y - note_names_initial_mouse_y_);
                if (dx > 3.0 || dy > 3.0) {
                    if (dy > dx * 1.5) {
                        note_names_pan_active_ = true;
                    } else {
                        vertical_zoom_active_ = true;
                        vertical_zoom_start_x_ = note_names_initial_mouse_x_;
                        vertical_zoom_start_pixels_per_key_ =
                            coords_.key_height();
                    }
                }
            }

            if (note_names_pan_active_) {
                // Inverted direction: drag down moves view up.
                double delta_y = -(local_y - note_names_start_y_);
                double new_viewport_y =
                    note_names_start_viewport_y_ + delta_y;
                coords_.set_scroll(coords_.viewport().x, new_viewport_y);
                // Update start state for continuous dragging.
                note_names_start_y_ = local_y;
                note_names_start_viewport_y_ = coords_.viewport().y;
            } else if (vertical_zoom_active_) {
                double delta_x = local_x - vertical_zoom_start_x_;
                double zoom_sensitivity = 0.01;
                double zoom_factor =
                    1.0 + (delta_x * zoom_sensitivity);
                double new_ppk =
                    vertical_zoom_start_pixels_per_key_ * zoom_factor;

                const double BASE_PPK = 20.0;
                const double MIN_ZOOM_PERCENT = 0.60;  // 60%
                const double MAX_ZOOM_PERCENT = 1.25;  // 125%
                double min_ppk = BASE_PPK * MIN_ZOOM_PERCENT;
                double max_ppk = BASE_PPK * MAX_ZOOM_PERCENT;
                if (new_ppk < min_ppk) new_ppk = min_ppk;
                if (new_ppk > max_ppk) new_ppk = max_ppk;

                // Anchor-based vertical zoom adapted from Python implementation.
                double old_ppk = coords_.key_height();
                double old_viewport_y = coords_.viewport().y;

                double view_height =
                    coords_.viewport().height -
                    static_cast<double>(top_padding_ + ruler_height_ +
                                        footer_height_);
                if (view_height <= 0.0) {
                    view_height = coords_.viewport().height;
                }

                double content_top =
                    static_cast<double>(top_padding_ + ruler_height_);
                double anchor_fraction =
                    (vertical_zoom_anchor_y_ - content_top) / view_height;
                if (anchor_fraction < 0.0) anchor_fraction = 0.0;
                if (anchor_fraction > 1.0) anchor_fraction = 1.0;

                double old_visible_keys = view_height / old_ppk;
                double total_keys =
                    static_cast<double>(coords_.total_keys());
                double old_top_key =
                    total_keys - 1.0 - (old_viewport_y / old_ppk);
                double anchor_key_fractional =
                    old_top_key - (anchor_fraction * old_visible_keys);

                coords_.set_key_height(new_ppk);

                double new_visible_keys = view_height / new_ppk;
                double new_top_key =
                    anchor_key_fractional +
                    (anchor_fraction * new_visible_keys);
                double new_viewport_y =
                    (total_keys - 1.0 - new_top_key) * new_ppk;

                coords_.set_scroll(coords_.viewport().x, new_viewport_y);
            }
        }
    }

    if (left_released && note_names_interaction_active_) {
        note_names_interaction_active_ = false;
        note_names_pan_active_ = false;
        vertical_zoom_active_ = false;
    }

    // Track clicked grid cell for debug visualization, matching Python
    // last_clicked_cell behaviour.
    if (left_clicked) {
        bool in_grid_x =
            local_x >= static_cast<float>(coords_.piano_key_width());
        bool in_grid_y =
            local_y >= top_padding_ + ruler_height_ &&
            (!config_.show_cc_lane || local_y < lane_top_local);
        if (in_grid_x && in_grid_y) {
            auto [world_x, world_y] =
                coords_.screen_to_world(local_x, local_y);

            double beat_number =
                world_x / coords_.pixels_per_beat();
            int snap_beat =
                beat_number >= 0.0
                    ? static_cast<int>(beat_number)
                    : 0;
            Tick tick_start =
                static_cast<Tick>(snap_beat *
                                  coords_.ticks_per_beat());
            Tick tick_end =
                tick_start + coords_.ticks_per_beat();

            double key_index =
                world_y / coords_.key_height();
            int key_from_top =
                static_cast<int>(key_index);
            int key =
                coords_.total_keys() - 1 - key_from_top;

            if (key >= 0 &&
                key < coords_.total_keys()) {
                has_last_clicked_cell_ = true;
                last_clicked_tick_start_ = tick_start;
                last_clicked_tick_end_ = tick_end;
                last_clicked_key_ =
                    static_cast<MidiKey>(key);
            } else {
                has_last_clicked_cell_ = false;
            }
        } else {
            has_last_clicked_cell_ = false;
        }

        // Piano-key press: click within piano key area between note labels and grid.
        bool in_piano_keys_x =
            local_x >= note_label_width_ &&
            local_x < static_cast<float>(coords_.piano_key_width());
        bool in_piano_keys_y =
            local_y >= top_padding_ + ruler_height_ &&
            (!config_.show_cc_lane || local_y < lane_top_local);
        if (in_piano_keys_x && in_piano_keys_y) {
            auto [world_x_pk, world_y_pk] =
                coords_.screen_to_world(local_x, local_y);
            (void)world_x_pk;
            MidiKey key_pk =
                coords_.world_y_to_key(world_y_pk);
            pressed_piano_key_ = key_pk;
            has_pressed_piano_key_ = true;
            piano_key_pressed_active_ = true;
            piano_key_flash_timer_ = piano_key_flash_duration_;
            if (on_piano_key_pressed_) {
                on_piano_key_pressed_(key_pk);
            }
        } else {
            has_pressed_piano_key_ = false;
            piano_key_pressed_active_ = false;
        }
    }

    // Update piano-key hover state.
    {
        bool in_piano_keys_x =
            local_x >= note_label_width_ &&
            local_x < static_cast<float>(coords_.piano_key_width());
        bool in_piano_keys_y =
            local_y >= top_padding_ + ruler_height_ &&
            (!config_.show_cc_lane || local_y < lane_top_local);
        if (in_piano_keys_x && in_piano_keys_y) {
            auto [world_x_pk, world_y_pk] =
                coords_.screen_to_world(local_x, local_y);
            (void)world_x_pk;
            hovered_piano_key_ =
                coords_.world_y_to_key(world_y_pk);
            has_hovered_piano_key_ = true;
        } else {
            has_hovered_piano_key_ = false;
        }
    }

    // First let the scrollbar handle events in its track area.
    handle_scrollbar_events();

    if (in_cc_lane && active_cc_lane_ >= 0 &&
        active_cc_lane_ < static_cast<int>(cc_lanes_.size())) {
        handle_cc_pointer_events(local_x,
                                 local_y,
                                 lane_top_local,
                                 lane_bottom_local,
                                 mods);
    } else {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            pointer_.on_mouse_down(MouseButton::Left,
                                   local_x,
                                   local_y,
                                   mods);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            pointer_.on_mouse_up(MouseButton::Left,
                                 local_x,
                                 local_y,
                                 mods);
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (pointer_.has_selection_rectangle()) {
                check_rectangle_edge_scrolling(local_x, local_y);
            }
            pointer_.on_mouse_move(local_x, local_y, mods);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            pointer_.on_double_click(MouseButton::Left,
                                     local_x,
                                     local_y,
                                     mods);
        }

         if (left_released && piano_key_pressed_active_) {
             piano_key_pressed_active_ = false;
             if (on_piano_key_released_) {
                 on_piano_key_released_(pressed_piano_key_);
             }
         }
    }
#endif
}

bool PianoRollWidget::check_rectangle_edge_scrolling(float local_x,
                                                     float local_y) {
#ifdef PIANO_ROLL_USE_IMGUI
    // Port of UnifiedPianoRoll._check_rectangle_edge_scrolling, adapted to
    // local widget coordinates.
    const Viewport& vp = coords_.viewport();
    float widget_width =
        static_cast<float>(coords_.piano_key_width() + vp.width);
    float widget_height = static_cast<float>(vp.height);

    const float margin = 60.0f;
    const float base_speed = 5.0f;
    const float max_speed = 25.0f;

    float left_edge =
        static_cast<float>(coords_.piano_key_width()) + margin;
    float right_edge = widget_width - margin;
    float top_edge = top_padding_ + ruler_height_ + margin;
    float bottom_edge =
        widget_height - footer_height_ - h_scrollbar_.track_size - margin;

    double h_scroll = 0.0;
    double v_scroll = 0.0;

    if (local_x < left_edge) {
        float distance = left_edge - local_x;
        h_scroll = -(base_speed + (distance / 20.0f) * 30.0f);
        if (h_scroll < -max_speed) h_scroll = -max_speed;
    } else if (local_x > right_edge) {
        float distance = local_x - right_edge;
        h_scroll = base_speed + (distance / 20.0f) * 30.0f;
        if (h_scroll > max_speed) h_scroll = max_speed;
    }

    if (local_y < top_edge) {
        float distance = top_edge - local_y;
        v_scroll = -(base_speed + (distance / 20.0f) * 30.0f);
        if (v_scroll < -max_speed) v_scroll = -max_speed;
    } else if (local_y > bottom_edge) {
        float distance = local_y - bottom_edge;
        v_scroll = base_speed + (distance / 20.0f) * 30.0f;
        if (v_scroll > max_speed) v_scroll = max_speed;
    }

    if (h_scroll != 0.0 || v_scroll != 0.0) {
        double new_x = coords_.viewport().x + h_scroll;
        double new_y = coords_.viewport().y + v_scroll;
        coords_.set_scroll(new_x, new_y);
        expand_explored_area(new_x);
        update_scrollbar_geometry();
        return true;
    }
    return false;
#else
    (void)local_x;
    (void)local_y;
    return false;
#endif
}

void PianoRollWidget::set_active_cc_lane_index(int index) noexcept {
    if (index < 0 ||
        index >= static_cast<int>(cc_lanes_.size())) {
        active_cc_lane_ = -1;
    } else {
        active_cc_lane_ = index;
    }
}

void PianoRollWidget::set_loop_enabled(bool enabled) noexcept {
    loop_markers_.enabled = enabled;
    loop_markers_.visible = enabled;
}

void PianoRollWidget::set_loop_range(Tick start,
                                     Tick end) noexcept {
    loop_markers_.set_tick_range(start, end);
}

void PianoRollWidget::handle_cc_pointer_events(float local_x,
                                               float local_y,
                                               float lane_top_local,
                                               float lane_bottom_local,
                                               const ModifierKeys& mods) {
#ifdef PIANO_ROLL_USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    const bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool mouse_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    float lane_height = lane_bottom_local - lane_top_local;
    if (lane_height <= 0.0f) {
        return;
    }

    if (active_cc_lane_ < 0 ||
        active_cc_lane_ >= static_cast<int>(cc_lanes_.size())) {
        return;
    }
    ControlLane& lane =
        cc_lanes_[static_cast<std::size_t>(active_cc_lane_)];

    // Map y to CC value (0 at bottom, 127 at top).
    float t = (local_y - lane_top_local) / lane_height;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int cc_value = static_cast<int>((1.0f - t) * 127.0f + 0.5f);

    // Map x to tick using CoordinateSystem (then apply magnetic snap, mirroring
    // note snapping including Shift to temporarily disable snapping).
    auto [world_x, /*world_y*/ _] =
        coords_.screen_to_world(local_x, 0.0);
    Tick tick_raw = coords_.world_to_tick(world_x);
    Tick tick = tick_raw;
    if (!mods.shift) {
        double ppb = coords_.pixels_per_beat();
        auto snapped =
            snap_.magnetic_snap(tick_raw, ppb);
        tick = snapped.first;
    }

    // Use a small threshold in ticks to find nearby points.
    Tick threshold = coords_.ticks_per_beat() / 16;  // ~1/16 note

    if (mouse_clicked) {
        // Ctrl-click near a point deletes it.
        if (io.KeyCtrl) {
            if (lane.remove_near(tick, threshold)) {
                return;
            }
        }

        // Try to start dragging an existing point.
        int idx = lane.index_near(tick, threshold);
        if (idx >= 0) {
            cc_dragging_ = true;
            cc_drag_index_ = idx;
            lane.set_value(idx, cc_value);
            return;
        }

        // Otherwise create a new point.
        lane.add_point(tick, cc_value);
        cc_dragging_ = false;
        cc_drag_index_ = -1;
        return;
    }

    if (mouse_down && cc_dragging_ && cc_drag_index_ >= 0) {
        // Whilst dragging, update tick and value.
        lane.set_value(cc_drag_index_, cc_value);
        lane.set_tick(cc_drag_index_, tick);
    }

    if (mouse_released) {
        cc_dragging_ = false;
        cc_drag_index_ = -1;
    }
#else
    (void)local_x;
    (void)local_y;
    (void)lane_top_local;
    (void)lane_bottom_local;
    (void)mods;
#endif
}

void PianoRollWidget::handle_keyboard_events() {
#ifdef PIANO_ROLL_USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    ModifierKeys mods{
        .shift = io.KeyShift,
        .ctrl = io.KeyCtrl,
        .alt = io.KeyAlt,
    };

    auto map_key = [](ImGuiKey imgui_key,
                      Key logical) -> bool {
        return ImGui::IsKeyPressed(imgui_key) ? true : false;
    };

    if (map_key(ImGuiKey_Delete, Key::Delete)) {
        keyboard_.on_key_press(Key::Delete, mods);
    }
    if (map_key(ImGuiKey_Backspace, Key::Backspace)) {
        keyboard_.on_key_press(Key::Backspace, mods);
    }
    if (map_key(ImGuiKey_A, Key::A)) {
        keyboard_.on_key_press(Key::A, mods);
    }
    if (map_key(ImGuiKey_C, Key::C)) {
        keyboard_.on_key_press(Key::C, mods);
    }
    if (map_key(ImGuiKey_V, Key::V)) {
        keyboard_.on_key_press(Key::V, mods);
    }
    if (map_key(ImGuiKey_Z, Key::Z)) {
        keyboard_.on_key_press(Key::Z, mods);
    }
    if (map_key(ImGuiKey_Y, Key::Y)) {
        keyboard_.on_key_press(Key::Y, mods);
    }

    bool moved = false;
    if (map_key(ImGuiKey_UpArrow, Key::Up)) {
        moved = keyboard_.on_key_press(Key::Up, mods) || moved;
    }
    if (map_key(ImGuiKey_DownArrow, Key::Down)) {
        moved = keyboard_.on_key_press(Key::Down, mods) || moved;
    }
    if (map_key(ImGuiKey_LeftArrow, Key::Left)) {
        moved = keyboard_.on_key_press(Key::Left, mods) || moved;
    }
    if (map_key(ImGuiKey_RightArrow, Key::Right)) {
        moved = keyboard_.on_key_press(Key::Right, mods) || moved;
    }

    if (moved) {
        ensure_selected_notes_visible();
    }
#endif
}

void PianoRollWidget::ensure_selected_notes_visible() {
    const auto& ns = notes_.notes();
    if (ns.empty()) {
        return;
    }

    bool any_selected = false;
    Tick min_tick = 0;
    Tick max_tick = 0;
    MidiKey min_key = 0;
    MidiKey max_key = 0;

    for (const Note& n : ns) {
        if (!n.selected) {
            continue;
        }
        if (!any_selected) {
            any_selected = true;
            min_tick = n.tick;
            max_tick = n.end_tick();
            min_key = n.key;
            max_key = n.key;
        } else {
            if (n.tick < min_tick) min_tick = n.tick;
            if (n.end_tick() > max_tick) max_tick = n.end_tick();
            if (n.key < min_key) min_key = n.key;
            if (n.key > max_key) max_key = n.key;
        }
    }

    if (!any_selected) {
        return;
    }

    double min_x = coords_.tick_to_world(min_tick);
    double max_x = coords_.tick_to_world(max_tick);

    double top_y = coords_.key_to_world_y(max_key);
    double bottom_y =
        coords_.key_to_world_y(min_key) + coords_.key_height();

    const Viewport& vp = coords_.viewport();
    double viewport_x = vp.x;
    double viewport_y = vp.y;
    double viewport_width = vp.width;
    double viewport_height = vp.height;

    double new_x = viewport_x;
    double new_y = viewport_y;

    if (min_x < viewport_x) {
        new_x = min_x;
    } else if (max_x > viewport_x + viewport_width) {
        new_x = max_x - viewport_width;
    }

    if (top_y < viewport_y) {
        new_y = std::max(top_y, 0.0);
    } else if (bottom_y > viewport_y + viewport_height) {
        new_y = bottom_y - viewport_height;
    }

    if (new_x != viewport_x || new_y != viewport_y) {
        coords_.set_scroll(new_x, new_y);
        expand_explored_area(new_x);
        update_scrollbar_geometry();
    }
}

void PianoRollWidget::update_scrollbar_geometry() {
#ifdef PIANO_ROLL_USE_IMGUI
    // Use the current item rect (piano roll) to position the scrollbar
    // at the bottom of the widget.
    ImVec2 canvas_min = ImGui::GetItemRectMin();
    ImVec2 canvas_max = ImGui::GetItemRectMax();

    float widget_width = canvas_max.x - canvas_min.x;
    float widget_height = canvas_max.y - canvas_min.y;
    if (widget_width <= 0.0f || widget_height <= 0.0f) {
        return;
    }

    int x = static_cast<int>(canvas_min.x +
                             static_cast<float>(coords_.piano_key_width()));
    int length = static_cast<int>(widget_width -
                                  static_cast<float>(coords_.piano_key_width()));
    int y = static_cast<int>(canvas_max.y - h_scrollbar_.track_size);

    h_scrollbar_.update_geometry(x, y, length);

    // Keep viewport size in sync with current view; explored range is
    // managed by the scrollbar handler.
    const Viewport& vp = coords_.viewport();
    h_scrollbar_.set_viewport_size(vp.width);
    h_scrollbar_.set_scroll_position(vp.x);
#endif
}

void PianoRollWidget::handle_scrollbar_events() {
#ifdef PIANO_ROLL_USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    // Hit-test against scrollbar track area.
    double track_x = h_scrollbar_.scroll_position();  // scroll_position_ is world; not used for hit-test.
    (void)track_x;

    // Use the geometry we set in update_scrollbar_geometry.
    // Approximate by using track_pos_/track_size_px_.
    // CustomScrollbar stores these in screen coordinates.
    // We access them indirectly via bounds and track_size; for hit-test we rely
    // on bounds and track height.

    // For simplicity, forward all mouse events unconditionally; the scrollbar
    // will internally ignore ones outside its track.
    double mx = mouse.x;
    double my = mouse.y;

    // Hover/move.
    h_scrollbar_.handle_mouse_move(mx, my);

    // Press/release.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        h_scrollbar_.handle_mouse_down(mx, my, 0);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        h_scrollbar_.handle_mouse_up(mx, my, 0);
    }
#endif
}

void PianoRollWidget::handle_scrollbar_scroll(double new_scroll) {
    // Directly update viewport.x to avoid clamping negative positions, as in
    // the Python implementation.
    coords_.viewport().x = new_scroll;
}

void PianoRollWidget::handle_scrollbar_edge_resize(const char* edge,
                                                   double /*delta_x*/) {
    // Equivalent to Python ScrollbarHandler.handle_edge_resize.
    const auto& manual_pos = h_scrollbar_.manual_thumb_pos();
    const auto& manual_size = h_scrollbar_.manual_thumb_size();
    if (!manual_pos || !manual_size) {
        return;
    }

    const auto& track_pos = h_scrollbar_.track_pos();
    const auto& track_size_px = h_scrollbar_.track_size_px();

    double track_x = track_pos.first;
    double track_width = track_size_px.first;
    double thumb_x_relative = manual_pos->first - track_x;
    double thumb_width = manual_size->first;

    if (track_width <= 0.0 || thumb_width <= 0.0) {
        return;
    }

    // Calculate new pixels_per_beat based on thumb ratio.
    double explored_range = h_scrollbar_.explored_max() - h_scrollbar_.explored_min();
    double thumb_ratio = thumb_width / track_width;

    // Effective screen width: width of the grid area.
    double effective_screen_width = coords_.viewport().width;

    double old_scroll_x = coords_.viewport().x;
    double old_ppb = coords_.pixels_per_beat();
    int ticks_per_beat = coords_.ticks_per_beat();

    double explored_min_tick =
        (explored_min_x_ / std::max(old_ppb, 1e-6)) * ticks_per_beat;
    double explored_max_tick =
        (explored_max_x_ / std::max(old_ppb, 1e-6)) * ticks_per_beat;
    double explored_tick_span =
        std::max(1e-6, explored_max_tick - explored_min_tick);

    double new_viewport_tick_span =
        std::max(1e-6, thumb_ratio * explored_tick_span);
    double new_ppb =
        (effective_screen_width * ticks_per_beat) / new_viewport_tick_span;
    new_ppb = std::max(10.0, std::min(500.0, new_ppb));

    double anchor_screen_x =
        (edge && edge[0] == 'l') ? effective_screen_width : 0.0;

    double anchor_world_old = old_scroll_x + anchor_screen_x;
    double anchor_tick =
        (anchor_world_old / std::max(old_ppb, 1e-6)) * ticks_per_beat;

    coords_.set_pixels_per_beat(new_ppb);
    double new_scroll_x =
        (anchor_tick / ticks_per_beat) * new_ppb - anchor_screen_x;

    // Do not clamp during edge-resize; instead expand explored area if needed.
    expand_explored_area(new_scroll_x);
    coords_.viewport().x = new_scroll_x;

    // Align explored range with new geometry.
    double viewport_world_width = effective_screen_width;
    double thumb_ratio_safe = std::max(1e-6, thumb_ratio);
    double explored_range_new = viewport_world_width / thumb_ratio_safe;
    double available_space = std::max(1.0, track_width - thumb_width);
    double scroll_norm =
        std::max(0.0, std::min(1.0, thumb_x_relative / available_space));
    explored_min_x_ = new_scroll_x -
                      scroll_norm * (explored_range_new - viewport_world_width);
    explored_max_x_ = explored_min_x_ + explored_range_new;

    h_scrollbar_.set_explored_area(explored_min_x_, explored_max_x_);
}

void PianoRollWidget::handle_scrollbar_double_click() {
    // Equivalent to Python ScrollbarHandler.handle_double_click.
    Viewport& vp = coords_.viewport();
    double view_width = vp.width;
    int tpb = coords_.ticks_per_beat();

    if (clip_end_tick_ > clip_start_tick_) {
        Tick clip_ticks = clip_end_tick_ - clip_start_tick_;
        double clip_beats =
            static_cast<double>(clip_ticks) / static_cast<double>(tpb);
        double new_ppb = view_width / clip_beats;
        new_ppb = std::max(15.0, std::min(480.0, new_ppb));

        coords_.set_pixels_per_beat(new_ppb);
        vp.x = coords_.tick_to_world(clip_start_tick_);
        explored_min_x_ = coords_.tick_to_world(clip_start_tick_);
        explored_max_x_ = coords_.tick_to_world(clip_end_tick_);
    } else {
        coords_.set_pixels_per_beat(60.0);
        vp.x = 0.0;
        explored_min_x_ = 0.0;
        explored_max_x_ = view_width;
    }

    h_scrollbar_.set_explored_area(explored_min_x_, explored_max_x_);
    update_scrollbar_geometry();
}

void PianoRollWidget::handle_scrollbar_drag_end() {
    // After dragging, keep scrollbar geometry in sync.
    update_scrollbar_geometry();
}

void PianoRollWidget::expand_explored_area(double new_x) {
    // Equivalent to Python _expand_explored_area: expand the explored area
    // to include the viewport starting at new_x.
    const Viewport& vp = coords_.viewport();
    double viewport_world_width = vp.width;
    double viewport_right = new_x + viewport_world_width;

    if (new_x < explored_min_x_) {
        explored_min_x_ = new_x;
    }
    if (viewport_right > explored_max_x_) {
        explored_max_x_ = viewport_right;
    }

    h_scrollbar_.set_explored_area(explored_min_x_, explored_max_x_);
}

void PianoRollWidget::update_explored_area_for_notes() {
    // Equivalent to Python _update_explored_area_for_notes, translated for
    // NoteManager.
    const auto& note_list = notes_.notes();
    if (note_list.empty()) {
        return;
    }

    Tick leftmost_tick = note_list.front().tick;
    Tick rightmost_tick = note_list.front().end_tick();

    for (const Note& n : note_list) {
        leftmost_tick = std::min(leftmost_tick, n.tick);
        rightmost_tick = std::max(rightmost_tick, n.end_tick());
    }

    double leftmost_x = coords_.tick_to_world(leftmost_tick);
    double rightmost_x = coords_.tick_to_world(rightmost_tick);

    bool changed = false;
    if (leftmost_x < explored_min_x_) {
        explored_min_x_ = leftmost_x;
        changed = true;
    }
    if (rightmost_x > explored_max_x_) {
        explored_max_x_ = rightmost_x;
        changed = true;
    }

    if (changed) {
        h_scrollbar_.set_explored_area(explored_min_x_, explored_max_x_);
    }
}

}  // namespace piano_roll
