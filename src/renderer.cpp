#include <math.h>

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <array>
#include <fstream>
#include <iostream>
#include <json/json.hpp>
#include <queue>
#include <future>
#include <tuple>
using json = nlohmann::json;

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "renderer.hpp"
#include "request.hpp"
#include "utils.hpp"
#include "creators.hpp"

const int Renderer::kMaxFramesInFlight = 3;

std::vector<MTL::Buffer *> Renderer::buildBuffers(MTL::Device *_pDevice,
                                                  std::vector<Buffer> &geometries,
                                                  std::vector<unsigned char> &buffer) {
    std::vector<MTL::Buffer *> r;
    for (auto &g : geometries) {
        if (g.sizeofComponent == 1) {
            std::vector<unsigned char> temp(g.length * 2);
            unsigned char* b = buffer.data() + g.offset;
            int j = 0;
            for (int i = 0; i < temp.size(); i+=2) {
                temp[i] =  b[j];
                j++;
            }

            MTL::Buffer *VertexBuffer = _pDevice->newBuffer(g.length * 2, MTL::ResourceStorageModeManaged);
            memcpy(VertexBuffer->contents(), temp.data(), g.length * 2);
            VertexBuffer->didModifyRange(NS::Range::Make(0, VertexBuffer->length()));
            r.push_back(VertexBuffer);
        } else {
            MTL::Buffer *VertexBuffer = _pDevice->newBuffer(g.length, MTL::ResourceStorageModeManaged);
            memcpy(VertexBuffer->contents(), buffer.data() + g.offset, g.length);
            VertexBuffer->didModifyRange(NS::Range::Make(0, VertexBuffer->length()));
            r.push_back(VertexBuffer);
        }
    }

    for (auto &mesh : meshes) {
        int size = sizeof(Material) - 12;
        UniformBuffer = _pDevice->newBuffer(size, MTL::ResourceStorageModeManaged);
        memcpy(UniformBuffer->contents(), mesh.material, size);
        UniformBuffer->didModifyRange(NS::Range::Make(0, UniformBuffer->length()));
        uniforms.push_back(UniformBuffer);
    }

    return r;
};

Renderer::Renderer(MTL::Device *pDevice) : _pDevice(pDevice->retain()) {
    json data = getEntry();
    std::vector<unsigned char> buffer = getBuffer(data);

    std::vector<Image> images = buildImages(data);
    auto [center, b] = buildGeometry(data, buffer, geometries);
    modelSize = b;
    buildNode(data, matricies, center);
    buildMesh(data, meshes, geometries);
    buildTexture(images);
    buildShaders();
    buffers = buildBuffers(_pDevice, geometries, buffer);
    _pCommandQueue = _pDevice->newCommandQueue();
    buildDepthStencilStates();

    buildFrameData();
    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
}

void Renderer::buildTexture(std::vector<Image> &images) {
    std::vector<int> srgb;
    for (auto &mesh : meshes) {
        srgb.push_back(mesh.material->baseColorTexture);
        srgb.push_back(mesh.material->emissiveTexture);
    }
    int i = 0;
    for (auto &image : images) {
        bool hassrbg = std::find(srgb.begin(), srgb.end(), i) != srgb.end();
        MTL::TextureDescriptor *pTextureDesc = MTL::TextureDescriptor::alloc()->init();
        pTextureDesc->setWidth(image.width);
        pTextureDesc->setHeight(image.height);
        pTextureDesc->setPixelFormat(hassrbg ? MTL::PixelFormatRGBA8Unorm_sRGB : MTL::PixelFormatRGBA8Unorm);
        pTextureDesc->setTextureType(MTL::TextureType2D);
        pTextureDesc->setStorageMode(MTL::StorageModeManaged);
        pTextureDesc->setUsage(MTL::ResourceUsageSample | MTL::ResourceUsageRead);

        MTL::Texture *pTexture = _pDevice->newTexture(pTextureDesc);

        pTexture->replaceRegion(MTL::Region(0, 0, 0, image.width, image.height, 1), 0, image.buffer, image.width * 4);

        textures.push_back(pTexture);
        i++;
    }
}

void Renderer::buildShaders() {
    using NS::StringEncoding::UTF8StringEncoding;

    std::ifstream input("./shaders/base.metal");
    std::string shaderSrc(std::istreambuf_iterator<char>(input), {});

    NS::Error *pError = nullptr;
    MTL::Library *pLibrary =
        _pDevice->newLibrary(NS::String::string(shaderSrc.data(), UTF8StringEncoding), nullptr, &pError);
    if (!pLibrary) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    MTL::Function *pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::Function *pFragFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));

    MTL::RenderPipelineDescriptor *pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    pDesc->colorAttachments()->object(0)->setBlendingEnabled(true);
    pDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactor::BlendFactorSourceAlpha);
    pDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactor::BlendFactorOneMinusSourceAlpha);
    pDesc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperation::BlendOperationAdd);

    _pPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
    if (!_pPSO) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    pLibrary->release();
};

Renderer::~Renderer() {
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::buildFrameData() {
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i) {
        _pFrameData[i] = _pDevice->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeManaged);
    }
}

void Renderer::buildDepthStencilStates() {
    MTL::DepthStencilDescriptor *pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
    pDsDesc->setDepthWriteEnabled(true);

    _pDepthStencilState = _pDevice->newDepthStencilState(pDsDesc);

    pDsDesc->release();
}

void Renderer::draw(MTK::View *pView) {
    NS::AutoreleasePool *pPool = NS::AutoreleasePool::alloc()->init();

    ///
    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer *pFrameDataBuffer = _pFrameData[_frame];

    MTL::CommandBuffer *pCmd = _pCommandQueue->commandBuffer();
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    Renderer *pRenderer = this;
    pCmd->addCompletedHandler(^void(MTL::CommandBuffer *pCmd) {
      dispatch_semaphore_signal(pRenderer->_semaphore);
    });

    reinterpret_cast<FrameData *>(pFrameDataBuffer->contents())->angle = (_angle += 0.01f);
    pFrameDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(FrameData)));
    ///

    MTL::RenderPassDescriptor *pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder *pEnc = pCmd->renderCommandEncoder(pRpd);

    int i = 0;
    for (auto &mesh : meshes) {
        CameraData cameraData = camera(modelSize, glm::vec3{0, _angle += 0.0001f, 0}, matricies[i]);
        UniformBuffer = _pDevice->newBuffer(sizeof(cameraData), MTL::ResourceStorageModeManaged);
        memcpy(UniformBuffer->contents(), &cameraData, sizeof(CameraData));
        UniformBuffer->didModifyRange(NS::Range::Make(0, UniformBuffer->length()));

        pEnc->setCullMode(MTL::CullMode::CullModeNone);
        pEnc->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
        pEnc->setDepthStencilState(_pDepthStencilState);
        pEnc->setRenderPipelineState(_pPSO);
        pEnc->setVertexBuffer(buffers[mesh.geometry->position], 0, geometries[mesh.geometry->position].stride, 0);
        pEnc->setVertexBuffer(buffers[mesh.geometry->normal], 0, 1);
        pEnc->setVertexBuffer(buffers[mesh.geometry->uv], 0, 4);
        if (mesh.geometry->tangent != -1) {
            pEnc->setVertexBuffer(buffers[mesh.geometry->tangent], 0, 5);
        }

        pEnc->setVertexBuffer(pFrameDataBuffer, 0, 2);
        pEnc->setVertexBuffer(UniformBuffer, 0, 3);
        pEnc->setFragmentBuffer(UniformBuffer, 0, 0);
        pEnc->setFragmentBuffer(uniforms[i], 0, 1);
        pEnc->setFragmentTexture(textures[mesh.material->baseColorTexture], 0);
        pEnc->setFragmentTexture(textures[mesh.material->normalTexture], 1);
        pEnc->setFragmentTexture(textures[mesh.material->metallicRoughnessTexture], 2);
        if (mesh.material->emissiveTexture != -1) {
            pEnc->setFragmentTexture(textures[mesh.material->emissiveTexture], 3);
        }
        if (mesh.material->occlusionTexture != -1) {
            pEnc->setFragmentTexture(textures[mesh.material->occlusionTexture], 4);
        }
        // pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3) );
        pEnc->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
                                    geometries[mesh.geometry->index].count,
                                    geometries[mesh.geometry->index].sizeofComponent == 4
                                        ? MTL::IndexType::IndexTypeUInt32
                                        : MTL::IndexType::IndexTypeUInt16,
                                    buffers[mesh.geometry->index],
                                    0);
        i++;
    }
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();

    pPool->release();
}
