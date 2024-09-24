#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

struct FrameData {
    float angle;
};
struct CameraData {
    glm::mat4 Model;
    glm::mat4 View;
    glm::mat4 Projection;
    glm::mat4 normal;
    glm::vec3 dir;
};

struct Buffer {
    int offset;
    int length;
    int count;
    int sizeofComponent;
    int stride;
};

struct Image {
    int width;
    int height; 
    int channels;
    unsigned char *buffer;
};

struct Geometry {
    // std::vector<int> index;
    // std::vector<double> position;
    // std::vector<double> normal;

    int index;
    int position;
    int normal;
    int uv;
    int tangent;
};

struct Material {
    float baseColor[4]{1.0,1.0,1.0,1.0};
    float roughnessFactor;
    float metallicFactor;
    int baseColorTexture;
    int metallicRoughnessTexture;
    int normalTexture;
    int emissiveTexture = -1;
    int occlusionTexture = -1;

    // Material(std::vector<double> b) : baseColor(b) {}
};

struct Mesh {
    Material *material;
    Geometry *geometry;

    Mesh(Geometry *g, Material *m) : material(m), geometry(g) {}
};
