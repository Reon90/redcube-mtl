/*
 *
 * Copyright 2022 Apple Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <queue>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <simd/simd.h>

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <array>
#include <fstream>
#include <iostream>

#include "renderer.hpp"

class MyMTKViewDelegate : public MTK::ViewDelegate {
public:
    MyMTKViewDelegate(MTL::Device *pDevice);
    virtual ~MyMTKViewDelegate() override;
    virtual void drawInMTKView(MTK::View *pView) override;

private:
    Renderer *_pRenderer;
};

class MyAppDelegate : public NS::ApplicationDelegate {
public:
    ~MyAppDelegate();

    virtual void applicationWillFinishLaunching(NS::Notification *pNotification) override;
    virtual void applicationDidFinishLaunching(NS::Notification *pNotification) override;
    virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application *pSender) override;

private:
    NS::Window *_pWindow;
    MTK::View *_pMtkView;
    MTL::Device *_pDevice;
    MyMTKViewDelegate *_pViewDelegate = nullptr;
};

MyAppDelegate::~MyAppDelegate() {
    _pMtkView->release();
    _pWindow->release();
    _pDevice->release();
    delete _pViewDelegate;
}

void MyAppDelegate::applicationWillFinishLaunching(NS::Notification *pNotification) {
    NS::Application *pApp = reinterpret_cast<NS::Application *>(pNotification->object());
    pApp->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
}

void MyAppDelegate::applicationDidFinishLaunching(NS::Notification *pNotification) {
    CGRect frame = (CGRect){{100.0, 100.0}, {768.0, 768.0}};

    _pWindow = NS::Window::alloc()->init(
        frame, NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled, NS::BackingStoreBuffered, false);

    _pDevice = MTL::CreateSystemDefaultDevice();

    _pMtkView = MTK::View::alloc()->init(frame, _pDevice);
    _pMtkView->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    _pMtkView->setClearColor(MTL::ClearColor::Make(1.0, 1.0, 1.0, 1.0));

    _pViewDelegate = new MyMTKViewDelegate(_pDevice);
    _pMtkView->setDelegate(_pViewDelegate);

    _pWindow->setContentView(_pMtkView);
    _pWindow->setTitle(NS::String::string("Redcube", NS::StringEncoding::UTF8StringEncoding));

    _pWindow->makeKeyAndOrderFront(nullptr);

    NS::Application *pApp = reinterpret_cast<NS::Application *>(pNotification->object());
    pApp->activateIgnoringOtherApps(true);
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed(NS::Application *pSender) {
    return true;
}

MyMTKViewDelegate::MyMTKViewDelegate(MTL::Device *pDevice) : MTK::ViewDelegate(), _pRenderer(new Renderer(pDevice)) {}

MyMTKViewDelegate::~MyMTKViewDelegate() {
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView(MTK::View *pView) {
    _pRenderer->draw(pView);
}

int main(int argc, char *argv[]) {
    NS::AutoreleasePool *pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application *pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate(&del);
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}
