#include <math.h>
#include <array>
#include <fstream>
#include <iostream>
#include <json/json.hpp>
#include <queue>
#include <future>
#include <tuple>
using json = nlohmann::json;

#define STB_IMAGE_IMPLEMENTATION
#include "image/stb_image.h"

std::string MODEL_NAME = "StainedGlassLamp";
std::string BASE_URL =
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/" + MODEL_NAME + "/glTF/";

json getEntry() {
    json data;
    if (true) {
        std::string url = BASE_URL + MODEL_NAME + ".gltf";
        data = json::parse(*request(url));
    } else {
        std::ifstream f("../models/DamagedHelmet.gltf");
        data = json::parse(f);
    }

    return data;
}

std::vector<unsigned char> getBuffer(json &data) {
    std::string u = data["buffers"][0]["uri"];
    int l = data["buffers"][0]["byteLength"];
    std::vector<unsigned char> buffer;
    if (true) {
        std::string url2 = BASE_URL + u;
        std::string str = *request(url2);
        buffer = std::vector<unsigned char>(str.begin(), str.end());
    } else {
        std::ifstream input("../models/" + u, std::ios::binary);
        buffer = std::vector<unsigned char>(std::istreambuf_iterator<char>(input), {});
    }

    return buffer;
}

void buildMesh(json &data, std::vector<Mesh> &meshes, std::vector<Buffer> &geometries) {
    for (auto &mesh : data["meshes"]) {
        auto primitive = mesh["primitives"][0];
        int i = primitive["material"];
        auto material = data["materials"][i];
        int indices = primitive["indices"];
        int pos = primitive["attributes"]["POSITION"];
        int norm = primitive["attributes"]["NORMAL"];
        int uv = primitive["attributes"]["TEXCOORD_0"];
        int tangent = primitive["attributes"].value("TANGENT", -1);

        std::vector<double> def{1.0, 1.0, 1.0, 1.0};

        Material *m = new Material;
        Geometry *g = new Geometry{indices, pos, norm, uv, tangent};
        json pbr = material["pbrMetallicRoughness"];
        std::vector<double> baseColor = pbr.value("baseColorFactor", def);
        std::copy(baseColor.begin(), baseColor.end(), m->baseColor);
        m->roughnessFactor = pbr.value("roughnessFactor", 1);
        m->metallicFactor = pbr.value("metallicFactor", 1);
        if (pbr.contains("baseColorTexture")) {
            m->baseColorTexture = pbr["baseColorTexture"]["index"];
        }
        if (material.contains("emissiveTexture")) {
            m->emissiveTexture = material["emissiveTexture"]["index"];
        }
        if (material.contains("occlusionTexture")) {
            m->occlusionTexture = material["occlusionTexture"]["index"];
        }
        if (material.contains("normalTexture")) {
            m->normalTexture = material["normalTexture"]["index"];
        }

        meshes.push_back(Mesh{g, m});
    }
}

void buildNode(json &data, std::vector<glm::mat4> &matricies, glm::vec3 &center) {
    for (auto &n : data["nodes"]) {
        std::queue<json> queue;

        queue.push(n);
        glm::mat4 root = glm::translate(glm::mat4(1.0f), -center);

        while (!queue.empty()) {
            nlohmann::json node = queue.front();
            queue.pop();
            if (node.contains("children")) {
                std::vector<int> children = node["children"];
                for (auto &t : children) {
                    queue.push(data["nodes"][t]);
                }
            }

            if (node.contains("matrix")) {
                std::vector<double> m = node["matrix"];
                glm::mat4 matrix = glm::make_mat4(m.data());
                root = root * matrix;
            }

            if (node.contains("rotation")) {
                glm::quat quat = glm::quat(node["rotation"][3], node["rotation"][0], node["rotation"][1], node["rotation"][2]);
                root = root * glm::toMat4(quat);
            }
            if (node.contains("scale")) {
                root = glm::scale(root, glm::vec3(node["scale"][0], node["scale"][1], node["scale"][2]));
            }
            // std::cout << glm::to_string(root) << std::endl;
            if (node.contains("translation")) {
                root = glm::translate(
                    root, glm::vec3(node["translation"][0], node["translation"][1], node["translation"][2]));
            }

            if (node.contains("mesh")) {
                matricies.push_back(root);
            }
        }
    }
}

std::tuple<glm::vec3, float> buildGeometry(json &data, std::vector<unsigned char> &buffer, std::vector<Buffer> &geometries) {
    glm::vec3 mMax{-INFINITY, -INFINITY, -INFINITY};
    glm::vec3 mMin{INFINITY, INFINITY, INFINITY};

    for (auto &accessor : data["accessors"]) {
        int v = accessor["bufferView"];
        auto bufferView = data["bufferViews"][v];

        int offset1 = bufferView.value("byteOffset", 0);
        int offset2 = accessor.value("byteOffset", 0);
        int stride = bufferView.value("byteStride", 0);
        int offset = offset1 + offset2;
        int length = (int)bufferView["byteLength"] - offset2;

        int count = accessor["count"];
        int sizeofComponent = getCount(accessor["componentType"]);
        int strideValue = 0;
        // int typeofComponent = getDataType(accessor["type"]);
        // int lengthByStride = (stride * count) / sizeofComponent;
        // int requiredLength = count * typeofComponent;
        // length = lengthByStride * sizeofComponent;

        // if (stride > 0) {
        //     if (buffer.size() < length + offset) {
        //         length -= offset2;
        //     }

        //     if (length != requiredLength) {
        //         // buffer is too big need to stride it
        //         // std::vector<uint8_t> stridedArr(requiredLength);
        //         // int j = 0;
        //         // for (int i = 0; i < requiredLength; i += typeofComponent) {
        //         //     for (int k = 0; k < typeofComponent; k++) {
        //         //         stridedArr[i + k] = buffer[j + k];
        //         //     }
        //         //     j += stride / sizeofComponent;
        //         // }
        //         strideValue = stride;
        //     }
        // }

        if (accessor.contains("min") && accessor.contains("max") && accessor["type"] == "VEC3") {
            glm::vec3 min = glm::vec3(accessor["min"][0], accessor["min"][1], accessor["min"][2]);
            glm::vec3 max = glm::vec3(accessor["max"][0], accessor["max"][1], accessor["max"][2]);
            mMin = glm::min(min, mMin);
            mMax = glm::max(max, mMax);
        }

        geometries.push_back(Buffer{offset, length, count, sizeofComponent, strideValue}); //* getCount(accessor["componentType"])});
    }

    glm::vec3 vec = mMax - mMin;
    glm::vec3 center = (mMax + mMin) * 0.5f;
    return std::make_tuple(center, glm::length(vec));
}

std::vector<Image> buildImages(json &data) {
    std::vector<std::future<std::vector<char>>> futures;
    std::vector<Image> images;
    for (auto &image : data["images"]) {
        std::string url = image["uri"];
        std::string url2 = BASE_URL + url;
        futures.push_back(std::async(std::launch::async, &download, url2));
    }
    for (size_t i = 0; i < futures.size(); ++i) {
       std::vector<char> res = futures[i].get();

        int width, height, nrChannels;
        unsigned char *buffer;
        buffer = stbi_load_from_memory((unsigned char *)res.data(), res.size(), &width, &height, &nrChannels, 4);
        // std::string url2 = "../models/" + url;
        // buffer = stbi_load(url2.data(), &width, &height, &nrChannels, 4);
        // std::cout << "unable to load image: " << stbi_failure_reason() << std::endl;
        images.push_back(Image(width, height, nrChannels, buffer));
    }
    return images;
}
