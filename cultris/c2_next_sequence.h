#pragma once

#include <string>

// The current piece is supplied separately, with its x/y/r state. Its virtual
// successor comes first. A preview piece exists only when enabled and known;
// append that piece's virtual successor only after actually adding the piece.
inline std::string c2_next_sequence(bool virtual_enabled, bool preview_enabled,
                                    char preview_piece) {
    std::string sequence;
    if (virtual_enabled) sequence.push_back('?');
    if (preview_enabled && preview_piece != '\0') {
        sequence.push_back(preview_piece);
        if (virtual_enabled) sequence.push_back('?');
    }
    return sequence;
}
