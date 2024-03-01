#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <chrono>
#include <iostream>
#include <thread>

const uint32_t vert_spv[] = {
#include "vert.spv"
};

const uint32_t frag_spv[] = {
#include "frag.spv"
};

struct Vertex {
  glm::vec3 pos;
  glm::u8vec4 color;
};

int main() {
  vk::raii::Context ctx{};

  vk::ApplicationInfo appInfo{nullptr, {}, nullptr, {}, vk::ApiVersion11};

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

  uint32_t glfwExtensionCount = 0;
  auto *exts = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  vk::raii::Instance instance{
      ctx, vk::InstanceCreateInfo{
               {}, &appInfo, 0, nullptr, glfwExtensionCount, exts}};

  const char *exts_dev[] = {vk::KHRMaintenance5ExtensionName,
                            vk::KHRDynamicRenderingExtensionName,
                            vk::KHRDepthStencilResolveExtensionName,
                            vk::KHRCreateRenderpass2ExtensionName,
                            vk::EXTRobustness2ExtensionName,
                            vk::KHRSwapchainExtensionName,
                            vk::EXTExtendedDynamicStateExtensionName};

  vk::raii::PhysicalDevice physicalDevice{
      instance.enumeratePhysicalDevices().front()};

  float queuePriorities[] = {1.0f};
  vk::DeviceQueueCreateInfo queueCreateInfo{{}, 0, queuePriorities};

  vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceMaintenance5FeaturesKHR,
                     vk::PhysicalDeviceDynamicRenderingFeatures,
                     vk::PhysicalDeviceRobustness2FeaturesEXT>
      dev_create_info{
          {{}, queueCreateInfo, {}, exts_dev, nullptr}, {}, {}, {}, {}};

  dev_create_info.get<vk::PhysicalDeviceFeatures2>()
      .features.setRobustBufferAccess(true);
  dev_create_info.get<vk::PhysicalDeviceMaintenance5FeaturesKHR>()
      .setMaintenance5(true);
  dev_create_info.get<vk::PhysicalDeviceDynamicRenderingFeatures>()
      .setDynamicRendering(true);
  dev_create_info.get<vk::PhysicalDeviceRobustness2FeaturesEXT>()
      .setRobustBufferAccess2(true);

  vk::raii::Device device{physicalDevice, dev_create_info.get<>()};

  VkSurfaceKHR surface_;
  if (glfwCreateWindowSurface(*instance, window, nullptr, &surface_) < 0)
    throw std::runtime_error("failed to create window surface!");
  vk::raii::SurfaceKHR surface(instance, surface_);

  vk::raii::SwapchainKHR swapchain(
      device,
      vk::SwapchainCreateInfoKHR{{},
                                 *surface,
                                 3,
                                 vk::Format::eB8G8R8A8Unorm,
                                 vk::ColorSpaceKHR::eSrgbNonlinear,
                                 vk::Extent2D{800, 600},
                                 1,
                                 vk::ImageUsageFlagBits::eColorAttachment,
                                 vk::SharingMode::eExclusive,
                                 0,
                                 nullptr,
                                 vk::SurfaceTransformFlagBitsKHR::eIdentity,
                                 vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                 vk::PresentModeKHR::eFifo,
                                 true});

  vk::raii::Queue queue{device.getQueue(0, 0)};

  vk::raii::Semaphore imageAvailableSemaphore{device,
                                              vk::SemaphoreCreateInfo{}};
  vk::raii::Semaphore renderFinishedSemaphore{device,
                                              vk::SemaphoreCreateInfo{}};

  vk::raii::CommandPool commandPool{
      device, vk::CommandPoolCreateInfo{
                  vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                      vk::CommandPoolCreateFlagBits::eTransient,
                  0}};

  std::vector<vk::Image> images = swapchain.getImages();
  std::vector<vk::raii::ImageView> imageViews;
  for (auto &image : images) {
    imageViews.emplace_back(
        device,
        vk::ImageViewCreateInfo{{},
                                image,
                                vk::ImageViewType::e2D,
                                vk::Format::eB8G8R8A8Unorm,
                                {},
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});
  }

  vk::raii::PipelineLayout pipelineLayout{device, {{}, 0, nullptr}};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo,
                     vk::PipelineCreateFlags2CreateInfoKHR>
      pci;

#if 1
  vk::StructureChain<vk::PipelineShaderStageCreateInfo,
                     vk::ShaderModuleCreateInfo>
      stages_storage[] = {
          {{{}, vk::ShaderStageFlagBits::eVertex, {}, "main"}, {{}, vert_spv}},
          {{{}, vk::ShaderStageFlagBits::eFragment, {}, "main"},
           {{}, frag_spv}}};

  vk::PipelineShaderStageCreateInfo stages[] = {stages_storage[0].get<>(),
                                                stages_storage[1].get<>()};
#else
  vk::raii::ShaderModule vert{device, {{}, vert_spv}};
  vk::raii::ShaderModule frag{device, {{}, frag_spv}};

  vk::PipelineShaderStageCreateInfo stages[] = {
      {{}, vk::ShaderStageFlagBits::eVertex, *vert, "main"},
      {{}, vk::ShaderStageFlagBits::eFragment, *frag, "main"}};
#endif

  vk::VertexInputBindingDescription bindingDescription{
      0, sizeof(Vertex), vk::VertexInputRate::eVertex};
  vk::VertexInputAttributeDescription attributeDescriptions[] = {
      {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
      {1, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, color)}};
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {}, bindingDescription, attributeDescriptions};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      {}, vk::PrimitiveTopology::eTriangleList, false};

  vk::PipelineViewportStateCreateInfo viewportState{{}, 1, nullptr, 1, nullptr};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      {},
      false,
      false,
      vk::PolygonMode::eFill,
      vk::CullModeFlagBits::eNone,
      vk::FrontFace::eCounterClockwise,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f};

  vk::PipelineMultisampleStateCreateInfo multisampling{};
  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      true,
      vk::BlendFactor::eSrcAlpha,
      vk::BlendFactor::eOneMinusSrcAlpha,
      vk::BlendOp::eAdd,
      vk::BlendFactor::eOne,
      vk::BlendFactor::eZero,
      vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineColorBlendStateCreateInfo colorBlending{
      {}, false, vk::LogicOp::eCopy, colorBlendAttachment};

  vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport,
                                      vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{{}, 2, dynamicStates};

  pci.get<vk::GraphicsPipelineCreateInfo>()
      .setFlags({}) // static_cast<vk::PipelineCreateFlags>(~0u)) // RADV driver
                    // (24.0.1) crashes with this
      .setStages(stages)
      .setPVertexInputState(&vertexInputInfo)
      .setPInputAssemblyState(&inputAssembly)
      .setPViewportState(&viewportState)
      .setPRasterizationState(&rasterizer)
      .setPMultisampleState(&multisampling)
      .setPDepthStencilState(&depthStencil)
      .setPColorBlendState(&colorBlending)
      .setPDynamicState(&dynamicState)
      .setLayout(*pipelineLayout)
      .setBasePipelineIndex(0);

  vk::Format formats[] = {vk::Format::eB8G8R8A8Unorm};
  pci.get<vk::PipelineRenderingCreateInfo>().setColorAttachmentFormats(formats);

  pci.get<vk::PipelineCreateFlags2CreateInfoKHR>().setFlags(
      vk::PipelineCreateFlagBits2KHR::eAllowDerivatives);

  vk::raii::Pipeline pipeline{device, nullptr, pci.get<>()};

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    commandPool.reset();

    auto [_, idx] = swapchain.acquireNextImage(
        UINT64_MAX, *imageAvailableSemaphore, nullptr);

    auto cbs = device.allocateCommandBuffers(
        {*commandPool, vk::CommandBufferLevel::ePrimary, 1});
    auto &cb = cbs.front();

    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    cb.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
        vk::ImageMemoryBarrier{vk::AccessFlagBits::eNone,
                               vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eColorAttachmentRead,
                               vk::ImageLayout::eUndefined,
                               vk::ImageLayout::eColorAttachmentOptimal,
                               vk::QueueFamilyIgnored,
                               vk::QueueFamilyIgnored,
                               images[idx],
                               {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

    cb.setViewport(0, {vk::Viewport{0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f}});
    cb.setScissor(0, {vk::Rect2D{{0, 0}, {800, 600}}});

    vk::StructureChain<vk::BufferCreateInfo, vk::BufferUsageFlags2CreateInfoKHR>
        indexBCI{vk::BufferCreateInfo{{},
                                      sizeof(uint32_t) * 6,
                                      static_cast<vk::BufferUsageFlags>(~0u),
                                      vk::SharingMode::eExclusive,
                                      0,
                                      nullptr},
                 {vk::BufferUsageFlagBits2KHR::eIndexBuffer}};
    vk::raii::Buffer index_buffer{device, indexBCI.get<>()};

    vk::StructureChain<vk::BufferCreateInfo, vk::BufferUsageFlags2CreateInfoKHR>
        vertexBCI{vk::BufferCreateInfo{{},
                                       sizeof(Vertex) * 4,
                                       static_cast<vk::BufferUsageFlags>(~0u),
                                       vk::SharingMode::eExclusive,
                                       0,
                                       nullptr},
                  {vk::BufferUsageFlagBits2KHR::eVertexBuffer}};
    vk::raii::Buffer vertex_buffer{device, vertexBCI.get<>()};

    vk::MemoryRequirements index_buffer_reqs =
        index_buffer.getMemoryRequirements();
    vk::MemoryRequirements vertex_buffer_reqs =
        vertex_buffer.getMemoryRequirements();

    uint32_t index_buffer_memory_type = 0;
    uint32_t vertex_buffer_memory_type = 0;

    auto mem_props = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
      if (index_buffer_reqs.memoryTypeBits & (1 << i) &&
          (mem_props.memoryTypes[i].propertyFlags &
           vk::MemoryPropertyFlagBits::eHostVisible)) {
        index_buffer_memory_type = i;
        break;
      }
    }

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
      if (vertex_buffer_reqs.memoryTypeBits & (1 << i) &&
          (mem_props.memoryTypes[i].propertyFlags &
           vk::MemoryPropertyFlagBits::eHostVisible)) {
        vertex_buffer_memory_type = i;
        break;
      }
    }

    vk::raii::DeviceMemory index_buffer_memory{
        device, vk::MemoryAllocateInfo{index_buffer_reqs.size,
                                       index_buffer_memory_type}};

    vk::raii::DeviceMemory vertex_buffer_memory{
        device, vk::MemoryAllocateInfo{vertex_buffer_reqs.size,
                                       vertex_buffer_memory_type}};

    index_buffer.bindMemory(*index_buffer_memory, 0);
    vertex_buffer.bindMemory(*vertex_buffer_memory, 0);

    uint32_t indices[] = {0, 1, 2, 3, 2, 999};
    Vertex vertices[] = {{{-0.5f, -0.5f, 0.0f}, {255, 0, 0, 255}},
                         {{0.5f, -0.5f, 0.0f}, {0, 255, 0, 255}},
                         {{0.5f, 0.5f, 0.0f}, {0, 0, 255, 255}},
                         {{-0.5f, 0.5f, 0.0f}, {255, 255, 255, 255}}};

    void *index_buffer_data = index_buffer_memory.mapMemory(0, sizeof(indices));
    memcpy(index_buffer_data, indices, sizeof(indices));
    index_buffer_memory.unmapMemory();

    void *vertex_buffer_data =
        vertex_buffer_memory.mapMemory(0, sizeof(vertices));
    memcpy(vertex_buffer_data, vertices, sizeof(vertices));
    vertex_buffer_memory.unmapMemory();

    // use VK_KHR_dynamic_rendering
    vk::ClearValue clearValue{
        vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
    vk::RenderingAttachmentInfo renderingAttachmentInfo{
        *imageViews[idx],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ResolveModeFlagBits::eNone,
        nullptr,
        vk::ImageLayout::eUndefined,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        clearValue};
    cb.beginRenderingKHR(vk::RenderingInfo{
        {}, {{0, 0}, {800, 600}}, 1, 0, renderingAttachmentInfo});

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    cb.bindIndexBuffer2KHR(*index_buffer, 0zu, sizeof(uint32_t) * 5,
                           vk::IndexType::eUint32);
    cb.bindVertexBuffers2EXT(0, *vertex_buffer, 0zu, sizeof(Vertex) * 4);
    cb.drawIndexed(6, 1, 0, 0, 0);

    cb.endRenderingKHR();

    cb.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {},
        vk::ImageMemoryBarrier{vk::AccessFlagBits::eColorAttachmentWrite,
                               vk::AccessFlagBits::eNone,
                               vk::ImageLayout::eColorAttachmentOptimal,
                               vk::ImageLayout::ePresentSrcKHR,
                               vk::QueueFamilyIgnored,
                               vk::QueueFamilyIgnored,
                               images[idx],
                               {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

    cb.end();

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo si{*imageAvailableSemaphore, waitStages, *cb,
                      *renderFinishedSemaphore};
    queue.submit(si);

    vk::ImageCreateInfo ici{{},
                            vk::ImageType::e2D,
                            vk::Format::eB8G8R8A8Unorm,
                            vk::Extent3D{800, 600, 1},
                            1,
                            1,
                            vk::SampleCountFlagBits::e1,
                            vk::ImageTiling::eLinear,
                            vk::ImageUsageFlagBits::eColorAttachment,
                            vk::SharingMode::eExclusive,
                            0,
                            nullptr,
                            vk::ImageLayout::eUndefined};

    vk::ImageSubresource2KHR isr{{vk::ImageAspectFlagBits::eColor, 0, 0}};

    auto sr_layout =
        device.getImageSubresourceLayoutKHR({&ici, &isr}).subresourceLayout;

    std::cout << "Subresource layout: " << sr_layout.offset << " "
              << sr_layout.size << " " << sr_layout.rowPitch << " "
              << sr_layout.arrayPitch << " " << sr_layout.depthPitch
              << std::endl;

    vk::Format framebuffer_format = vk::Format::eB8G8R8A8Unorm;
    auto granularity = device.getRenderingAreaGranularityKHR(
        vk::RenderingAreaInfoKHR{0, framebuffer_format});

    std::cout << "Granularity: " << granularity.width << "x"
              << granularity.height << std::endl;

    std::cout << "dynamic rendering ptr: "
              << device.getProcAddr("vkCmdBeginRendering") << std::endl;
    std::cout << "dynamic rendering KHR ptr: "
              << device.getProcAddr("vkCmdBeginRenderingKHR") << std::endl;

    (void)queue.presentKHR({*renderFinishedSemaphore, *swapchain, idx});
    device.waitIdle();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}