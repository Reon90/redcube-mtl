#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include "objects.hpp"

class Renderer {
public:
    Renderer(MTL::Device *pDevice);
    ~Renderer();
    void draw(MTK::View *pView);
    void buildShaders();
    void buildFrameData();
    void buildTexture(std::vector<Image>&);
    void buildDepthStencilStates();
    std::vector<MTL::Buffer *> buildBuffers(MTL::Device*,
                                            std::vector<Buffer>&,
                                            std::vector<unsigned char>&);

private:
    std::vector<Buffer> geometries;
    MTL::Device *_pDevice;
    MTL::CommandQueue *_pCommandQueue;
    MTL::DepthStencilState *_pDepthStencilState;
    MTL::RenderPipelineState *_pPSO;
    std::vector<MTL::Buffer *> buffers;
    std::vector<MTL::Buffer *> uniforms;
    MTL::Buffer *UniformBuffer;
    std::vector<Mesh> meshes;
    std::vector<glm::mat4> matricies;
    std::vector<MTL::Texture *> textures;
    float modelSize;

    MTL::Buffer *_pFrameData[3];
    float _angle;
    int _frame;
    dispatch_semaphore_t _semaphore;
    static const int kMaxFramesInFlight;
};
