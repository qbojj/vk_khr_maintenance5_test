#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <bit>
#include <iostream>

const uint32_t vert_spv[] = {
#include "vert.spv"
};

int main() {
  vk::raii::Context ctx{};
  vk::ApplicationInfo appInfo{nullptr, {}, nullptr, {}, vk::ApiVersion13};

  vk::raii::Instance instance{ctx, vk::InstanceCreateInfo{{}, &appInfo, 0}};

  const char *exts_dev[] = {vk::KHRMaintenance5ExtensionName};

  vk::raii::PhysicalDevice physicalDevice{
      instance.enumeratePhysicalDevices().front()};

  float queuePriorities[] = {1.0f};
  vk::DeviceQueueCreateInfo queueCreateInfo{{}, 0, queuePriorities};

  vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceMaintenance5FeaturesKHR,
                     vk::PhysicalDeviceDynamicRenderingFeatures>
      dev_create_info{{{}, queueCreateInfo, {}, exts_dev, nullptr}, {}, {}, {}};

  dev_create_info.get<vk::PhysicalDeviceMaintenance5FeaturesKHR>()
      .setMaintenance5(true);
  dev_create_info.get<vk::PhysicalDeviceDynamicRenderingFeatures>()
      .setDynamicRendering(true);

  vk::raii::Device device{physicalDevice, dev_create_info.get<>()};

  vk::raii::PipelineLayout pipelineLayout{device, {{}, 0, nullptr}};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo,
                     vk::PipelineCreateFlags2CreateInfoKHR>
      pci;

  vk::StructureChain<vk::PipelineShaderStageCreateInfo,
                     vk::ShaderModuleCreateInfo>
      stage = {{{}, vk::ShaderStageFlagBits::eVertex, {}, "main"},
               {{}, vert_spv}};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
  vk::PipelineViewportStateCreateInfo viewportState{{}, 1, nullptr, 1, nullptr};
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  vk::PipelineMultisampleStateCreateInfo multisampling{};
  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  std::array dynamicStates = {vk::DynamicState::eViewport,
                              vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{{}, dynamicStates};

  pci.get<vk::GraphicsPipelineCreateInfo>()
      .setFlags(vk::PipelineCreateFlagBits::eLibraryKHR)
      .setStages(stage.get<>())
      .setPVertexInputState(&vertexInputInfo)
      .setPInputAssemblyState(&inputAssembly)
      .setPViewportState(&viewportState)
      .setPRasterizationState(&rasterizer)
      .setPMultisampleState(&multisampling)
      .setPDepthStencilState(&depthStencil)
      .setPColorBlendState(&colorBlending)
      .setPDynamicState(&dynamicState)
      .setLayout(*pipelineLayout);

  pci.get<vk::PipelineRenderingCreateInfo>().setColorAttachmentFormats({});
  pci.get<vk::PipelineCreateFlags2CreateInfoKHR>().setFlags(
      vk::PipelineCreateFlagBits2KHR::eDisableOptimization);

  // crashes here
  vk::raii::Pipeline pipeline =
      device.createGraphicsPipeline(nullptr, pci.get<>());

  std::cout << "never reached\n";
}