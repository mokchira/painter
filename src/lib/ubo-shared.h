#include <coal/coal.h>

typedef struct {
    Mat4 model;
    Mat4 view;
    Mat4 proj;
    Mat4 viewInv;
    Mat4 projInv;
} UboMatrices;

typedef struct {
    float x;
    float y;
    float radius;
    float r;
    float g;
    float b;
    float opacity;
    float anti_falloff;
} UboBrush;

