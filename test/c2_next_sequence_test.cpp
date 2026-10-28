#include "../cultris/c2_next_sequence.h"

#include <cstdio>
#include <cstring>

int main() {
    struct Case { bool virtual_on; bool preview_on; char next; const char* expected; };
    const Case cases[] = {
        {false, false, 'T', ""}, {false, true, 'T', "T"},
        {true, false, 'T', "?"}, {true, true, 'T', "?T?"},
        {false, false, '\0', ""}, {false, true, '\0', ""},
        {true, false, '\0', "?"}, {true, true, '\0', "?"}
    };
    int failures = 0;
    for (const auto& c : cases) {
        const auto got = c2_next_sequence(c.virtual_on, c.preview_on, c.next);
        if (got != c.expected) {
            ++failures;
            std::printf("FAIL virtual=%d preview=%d next=%d expected=%s actual=%s\n",
                c.virtual_on, c.preview_on, c.next, c.expected, got.c_str());
        }
    }
    for (const char piece : {'I', 'T', 'S', 'Z', 'L', 'J', 'O'}) {
        const auto hidden = c2_next_sequence(true, false, piece);
        const auto shown = c2_next_sequence(true, true, piece);
        if (hidden != "?" || shown != std::string("?") + piece + "?") ++failures;
    }
    std::printf("%s c2_next_sequence_test (%d failures; 8 flag/unknown cases and 7 kinds)\n",
        failures == 0 ? "PASS" : "FAIL", failures);
    return failures != 0;
}
