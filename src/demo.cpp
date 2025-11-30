#include "piano_roll/demo.hpp"
#include "piano_roll/playback.hpp"

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

void RenderPianoRollDemo(NoteManager& note_manager,
                         CoordinateSystem& coords,
                         PianoRollRenderer* renderer_override) {
#ifdef PIANO_ROLL_USE_IMGUI
    static PianoRollRenderer default_renderer{};
    PianoRollRenderer& renderer =
        renderer_override ? *renderer_override : default_renderer;

    // Simple transport/playback state for the demo.
    static PlaybackState playback{};
    static bool playback_initialised = false;
    if (!playback_initialised) {
        playback.set_ticks_per_beat(coords.ticks_per_beat());
        playback.set_tempo(120.0);
        playback.set_position(0);
        playback.set_loop_enabled(false);
        playback_initialised = true;
    } else {
        // Keep ticks-per-beat in sync with the coordinate system.
        playback.set_ticks_per_beat(coords.ticks_per_beat());
    }

    // Adapt viewport size to available content region.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f || avail.y <= 0.0f) {
        return;
    }

    Viewport& vp = coords.viewport();
    vp.width = static_cast<double>(avail.x) - coords.piano_key_width();
    if (vp.width < 100.0) {
        vp.width = 100.0;
    }
    vp.height = static_cast<double>(avail.y);

    // Simple zoom control.
    double ppb = coords.pixels_per_beat();
    float zoom = static_cast<float>(ppb);
    if (ImGui::SliderFloat("Zoom (px/beat)", &zoom, 15.0f, 240.0f)) {
        coords.set_zoom(static_cast<double>(zoom));
    }

    // Clip colour control to exercise PianoRollRenderConfig::apply_clip_color
    // similar to UnifiedPianoRoll.set_clip_color.
    {
        static float clip_color[4] = {
            renderer.config().note_fill_color.r,
            renderer.config().note_fill_color.g,
            renderer.config().note_fill_color.b,
            renderer.config().note_fill_color.a,
        };
        if (ImGui::ColorEdit4("Clip Color", clip_color)) {
            ColorRGBA c{
                clip_color[0],
                clip_color[1],
                clip_color[2],
                clip_color[3]};
            renderer.config().apply_clip_color(c);
        }
    }

    // Basic transport controls for playback demonstration.
    {
        float tempo = static_cast<float>(playback.tempo_bpm);
        if (ImGui::SliderFloat("Tempo (BPM)", &tempo, 40.0f, 240.0f)) {
            playback.set_tempo(static_cast<double>(tempo));
        }

        if (ImGui::Button("Play")) {
            playback.play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            playback.pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            playback.pause();
            playback.set_position(0);
            renderer.clear_playhead();
        }

        // Advance playback and update playhead when playing.
        ImGuiIO& io = ImGui::GetIO();
        if (playback.playing) {
            Tick pos =
                playback.advance(static_cast<double>(io.DeltaTime));
            if (pos < 0) {
                pos = 0;
            }
            renderer.set_playhead(pos);
        }
    }

    // Render the piano roll directly below the controls.
    renderer.render(coords, note_manager);
#else
    (void)note_manager;
    (void)coords;
    (void)renderer_override;
    // Built without Dear ImGui: demo does nothing.
#endif
}

void RenderPianoRollDemo() {
#ifdef PIANO_ROLL_USE_IMGUI
    // Lazily initialise shared demo state (notes + coordinates).
    static bool initialised = false;
    static NoteManager note_manager;
    static CoordinateSystem coords{180.0};
    static PianoRollRenderer renderer{};

    if (!initialised) {
        // Seed a few simple notes (C major chord over 2 bars).
        const Tick bar = 4 * coords.ticks_per_beat();
        const Duration len = coords.ticks_per_beat();  // 1 beat
        const MidiKey c4 = 60;
        const MidiKey e4 = 64;
        const MidiKey g4 = 67;

        note_manager.create_note(0, len, c4);
        note_manager.create_note(0, len, e4);
        note_manager.create_note(0, len, g4);

        note_manager.create_note(bar, len, c4);
        note_manager.create_note(bar, len, e4);
        note_manager.create_note(bar, len, g4);

        // Set an initial viewport: show 4 bars and the C4 region.
        Viewport& vp = coords.viewport();
        vp.width = 800.0;
        vp.height = 400.0;
        coords.center_on_key(c4);
        coords.center_on_tick(0);

        initialised = true;
    }

    // Delegate to the generic variant using the static renderer.
    RenderPianoRollDemo(note_manager, coords, &renderer);
#else
    // Built without Dear ImGui: demo does nothing.
#endif
}

}  // namespace piano_roll
