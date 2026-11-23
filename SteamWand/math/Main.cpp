#include "Mat4.h"
#include "Vec3.h"
#include "Vec4.h"

#include <iostream>

#include "../physics/Collision.h"

int main(int argc, char* argv[]) {

    vec3 myVec = { 1, 2, 3 };
    vec3 myVec2 = { 4, 5, 6 };

    float d = myVec.dot(myVec2);

    //std::cout << "dot " << d;

    std::cout << " add " << add();

    return 0;
}
