#include "Mat4.h"
#include "Vec3.h"
#include "Vec4.h"

#include <iostream>

int main(int argc, char* argv[]) {

    vec3 myVec = { 1, 2, 3 };
    vec3 myVec2 = { 4, 5, 6 };

    float d = myVec.dot(myVec2);

    std::cout << "dot " << d;

    return 0;
}
