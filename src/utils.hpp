#include <string>

CameraData camera(float Translate, glm::vec3 const &Rotate, glm::mat4& matricies) {
    glm::mat4 Projection = glm::perspective(0.78f, 1.0f, 0.01f, 100.f);
    glm::mat4 View = glm::mat4(1.0);
    View = glm::rotate(View, Rotate.x, glm::vec3(1.0f, 0.0f, 0.0f));
    View = glm::rotate(View, Rotate.y, glm::vec3(0.0f, 1.0f, 0.0f));
    View = glm::rotate(View, Rotate.z, glm::vec3(0.0f, 0.0f, 1.0f));
    View = glm::translate(View, glm::vec3(0.0f, 0.0f, Translate));
    glm::vec3 dir = glm::vec3(View[3][0], View[3][1], View[3][2]);
    View = glm::inverse(View);
    glm::mat4 Model = matricies;
    glm::mat4 normal = glm::inverse(matricies);
    normal = glm::transpose(normal);
    return CameraData{Model, View, Projection, normal, dir};
}

int getCount(int type) {
    int arr;
    switch (type) {
        case 5120:
        case 5121:
            arr = 1;
            break;
        case 5122:
        case 5123:
            arr = 2;
            break;
        case 5124:
        case 5125:
        case 5126:
            arr = 4;
            break;
    }
    return arr;
}
int getDataType(std::string type) {
    int count;
    if (type == "MAT2") count = 4;

    if (type == "MAT3") count = 9;

    if (type == "MAT4") count = 16;

    if (type == "VEC4") count = 4;

    if (type == "VEC3") count = 3;

    if (type == "VEC2") count = 2;

    if (type == "SCALAR") count = 1;

    return count;
}
