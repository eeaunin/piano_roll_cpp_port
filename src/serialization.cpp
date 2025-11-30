#include "piano_roll/serialization.hpp"

#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace piano_roll {

void serialize_notes_and_cc(const NoteManager& notes,
                            const std::vector<ControlLane>& lanes,
                            std::ostream& out) {
    out << "PPR1\n";

    for (const Note& n : notes.notes()) {
        out << "N " << n.tick << " " << n.duration << " "
            << n.key << " " << n.velocity << " "
            << n.channel << "\n";
    }

    for (const ControlLane& lane : lanes) {
        int cc = lane.cc_number();
        for (const ControlPoint& p : lane.points()) {
            out << "C " << cc << " "
                << p.tick << " "
                << p.value << "\n";
        }
    }
}

void deserialize_notes_and_cc(NoteManager& notes,
                              std::vector<ControlLane>& lanes,
                              std::istream& in) {
    notes.clear();
    lanes.clear();

    std::unordered_map<int, std::size_t> cc_to_index;

    std::string line;
    bool first_line = true;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        char type = 0;
        iss >> type;
        if (!iss) {
            continue;
        }

        if (first_line && type == 'P') {
            // Expecting "PPR1"; ignore content.
            first_line = false;
            continue;
        }
        first_line = false;

        if (type == 'N') {
            Tick tick{};
            Duration dur{};
            int key{};
            int vel{};
            int chan{};
            if (!(iss >> tick >> dur >> key >> vel >> chan)) {
                continue;
            }
            notes.create_note(tick,
                              dur,
                              key,
                              vel,
                              chan,
                              /*selected=*/false,
                              /*record_undo=*/false,
                              /*allow_overlap=*/true);
        } else if (type == 'C') {
            int cc{};
            Tick tick{};
            int value{};
            if (!(iss >> cc >> tick >> value)) {
                continue;
            }

            auto it = cc_to_index.find(cc);
            if (it == cc_to_index.end()) {
                lanes.push_back(ControlLane{cc});
                std::size_t idx = lanes.size() - 1;
                cc_to_index.emplace(cc, idx);
                it = cc_to_index.find(cc);
            }

            lanes[it->second].add_point(tick, value);
        } else {
            // Unknown line type; ignore.
            continue;
        }
    }
}

}  // namespace piano_roll

