#pragma once

class Feedback {
    unsigned int black_;
    unsigned int white_;
public:
    Feedback(unsigned int black, unsigned int white) : black_(black), white_(white) {}
    inline unsigned int black() const { return black_; }
    inline unsigned int white() const { return white_; }
    inline bool operator==(const Feedback& other) const = default;
};

inline bool operator>(const Feedback& fb_a, const Feedback& fb_b) {
    if (fb_a.black() != fb_b.black())
        return fb_a.black() > fb_b.black();
    if (fb_a.white() != fb_b.white())
        return fb_a.white() > fb_b.white();
    return false;
}
