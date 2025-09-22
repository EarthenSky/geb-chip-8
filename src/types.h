#ifndef TYPES_H
#define TYPES_H

struct u4 {
    unsigned int value : 4;
    u4(size_t x) : value(x % 16) {}
    operator size_t() const { return this->value; }
};
struct u4u4 {
    unsigned int first : 4;
    unsigned int second : 4;
};

#endif