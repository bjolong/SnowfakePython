#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "../VulkanComputeHelper.h"
#include "Memory.h"
#include "ImageData.h"
#include "GraphicsPipeline.h"

namespace vkch = vkComputeHelper;

#define VOLUME_SWAPS 3

struct Volume
{
  Volume(
    const std::vector<uint32_t> &queue_family_indexes,
    vk::raii::PhysicalDevice const &physical_device,
    vk::raii::Device const &device,
    vk::Extent3D const &extent,
    vk::ImageLayout const &image_layout)
    : device_memory(nullptr)
    , image(nullptr)
    , image_view(nullptr)
    , sampler(nullptr)
  {
    image = std::move(
      vk::raii::Image(
        device, vk::ImageCreateInfo(
          vk::ImageCreateFlags(),
          vk::ImageType::e3D,
          vk::Format::eR16Snorm,
          extent,
          1, 1,
          vk::SampleCountFlagBits::e1,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eStorage |
          vk::ImageUsageFlagBits::eSampled,
          (queue_family_indexes.size() > 1) ?
            vk::SharingMode::eConcurrent :
            vk::SharingMode::eExclusive,
          queue_family_indexes,
          image_layout
        )
      )
    );

    device_memory = std::move(
      allocateDeviceMemory(
        device, physical_device.getMemoryProperties(),
        image.getMemoryRequirements(),
        vk::MemoryPropertyFlagBits::eDeviceLocal
      )
    );

    image.bindMemory(device_memory, 0);

    image_view = std::move(
      vk::raii::ImageView(
        device,
        vk::ImageViewCreateInfo(
          {},
          image,
          vk::ImageViewType::e3D,
          vk::Format::eR16Snorm,
          {},
          { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        )
      )
    );

    sampler = std::move(
      vk::raii::Sampler(
        device,
        vk::SamplerCreateInfo(
          vk::SamplerCreateFlags(),
          vk::Filter::eNearest,
          vk::Filter::eNearest,
          vk::SamplerMipmapMode::eNearest,
          vk::SamplerAddressMode::eClampToBorder,
          vk::SamplerAddressMode::eClampToBorder,
          vk::SamplerAddressMode::eClampToBorder,
          0.0f,
          false,
          16.0f,
          false,
          vk::CompareOp::eNever,
          0.0f,
          0.0f,
          vk::BorderColor::eFloatOpaqueBlack
        )
      )
    );
  }

  void addTransition(
    vk::raii::CommandBuffer const &command_buffer,
    vk::ImageLayout old_image_layout,
    vk::ImageLayout new_image_layout)
  {
    setImageLayout(
      command_buffer,
      *image,
      vk::Format::eR16Snorm,
      old_image_layout,
      new_image_layout);
  }

  vk::raii::DeviceMemory device_memory;
  vk::raii::Image image;
  vk::raii::ImageView image_view;
  vk::raii::Sampler sampler;
};

class VolumeBuffers
{
public:
  VolumeBuffers(
    vkch::Context &ctxt,
    unsigned int x_size,
    unsigned int y_size,
    unsigned int z_size)
    : _read_idx(0)
    , _write_idx(2)
    , _internal_state(state_flags::READ_AND_WRITE_GENERAL)
  {
    std::vector<uint32_t> queue_family_indexes;
    for (int i = 0; i < ctxt.auxiliaryQueueCount(); i++)
    {
      queue_family_indexes.push_back(
        ctxt.auxiliaryQueueFamily(i)
      );

    } // i
    queue_family_indexes.push_back(ctxt.computeQueueFamily());

    std::sort(queue_family_indexes.begin(), queue_family_indexes.end());
    std::vector<uint32_t>::iterator end_unique =
      std::unique(queue_family_indexes.begin(), queue_family_indexes.end());
    queue_family_indexes.erase(end_unique, queue_family_indexes.end());

    for (int i = 0; i < VOLUME_SWAPS; i++)
    {
      volumes[i] = std::make_unique<Volume>(
        queue_family_indexes,
        ctxt.physical_device(),
        ctxt.device(),
        vk::Extent3D(x_size, y_size, z_size),
        vk::ImageLayout::eUndefined
      );

    } // i

    // Make read descriptors
    {
      _read_descriptor_set_layout = std::make_unique<vk::raii::DescriptorSetLayout>(
        makeDescriptorSetLayout(
          ctxt.device(), {
            { vk::DescriptorType::eCombinedImageSampler, 1,
              vk::ShaderStageFlagBits::eFragment }
          }, vk::DescriptorSetLayoutCreateFlags()
        )
      );

      vk::DescriptorPoolSize descriptor_pool_size(
        vk::DescriptorType::eCombinedImageSampler, 1);

      for (int i = 0; i < VOLUME_SWAPS; i++)
      {
        vk::DescriptorPoolCreateInfo descriptor_pool_info(
          vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
          1, 1,
          &descriptor_pool_size
        );

        _read_descriptor_pool[i] = std::make_unique<vk::raii::DescriptorPool>(
          ctxt.device(), descriptor_pool_info
        );

        vk::DescriptorSetAllocateInfo descriptor_set_allocate_info(
          *(_read_descriptor_pool[i]), **_read_descriptor_set_layout
        );

        _read_descriptor_set[i] = std::make_unique<vk::raii::DescriptorSet>(
          std::move(
            vk::raii::DescriptorSets(
              ctxt.device(),
              descriptor_set_allocate_info).front()
          )
        );

        vk::DescriptorImageInfo descriptor_image_info(
          volumes[i]->sampler,
          volumes[i]->image_view,
          vk::ImageLayout::eGeneral
        );

        vk::WriteDescriptorSet write_read_descriptor_set(
          *(_read_descriptor_set[i]),
          0, // shader bind location
          0, 1, vk::DescriptorType::eCombinedImageSampler,
          &descriptor_image_info,
          nullptr);

        ctxt.device().updateDescriptorSets(write_read_descriptor_set, nullptr);

      } // i
    }

    // Make write descriptors
    {
      _write_descriptor_set_layout = std::make_unique<vk::raii::DescriptorSetLayout>(
        makeDescriptorSetLayout(
          ctxt.device(), {
            { vk::DescriptorType::eStorageImage, 1,
              vk::ShaderStageFlagBits::eCompute }
          }, vk::DescriptorSetLayoutCreateFlags()
        )
      );

      vk::DescriptorPoolSize descriptor_pool_size(
        vk::DescriptorType::eStorageImage, 1);

      for (int i = 0; i < VOLUME_SWAPS; i++)
      {
        vk::DescriptorPoolCreateInfo descriptor_pool_info(
          vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
          1, 1,
          &descriptor_pool_size
        );

        _write_descriptor_pool[i] = std::make_unique<vk::raii::DescriptorPool>(
          ctxt.device(), descriptor_pool_info
        );

        vk::DescriptorSetAllocateInfo descriptor_set_allocate_info(
          *(_write_descriptor_pool[i]), **_write_descriptor_set_layout
        );

        _write_descriptor_set[i] = std::make_unique<vk::raii::DescriptorSet>(
          std::move(
            vk::raii::DescriptorSets(
              ctxt.device(),
              descriptor_set_allocate_info).front()
          )
        );

        vk::DescriptorImageInfo descriptor_image_info(
          VK_NULL_HANDLE,
          volumes[i]->image_view,
          vk::ImageLayout::eGeneral
        );

        vk::WriteDescriptorSet write_write_descriptor_set(
          *(_write_descriptor_set[i]),
          0, // shader bind location
          0, 1, vk::DescriptorType::eStorageImage,
          &descriptor_image_info,
          nullptr);

        ctxt.device().updateDescriptorSets(write_write_descriptor_set, nullptr);

      } // i
    }
  }

  void initialise(
    vk::raii::Device const &device,
    vk::raii::CommandBuffer const &command_buffer,
    vk::raii::Queue const & queue,
    std::mutex *queue_lock)
  {
    oneTimeSubmitAndWaitToComplete(
      device, command_buffer, queue, queue_lock,
      [&](vk::raii::CommandBuffer const &command_buffer)
      {
        for (int i = 0; i < VOLUME_SWAPS; i++)
        {
          volumes[i]->addTransition(
            command_buffer,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral
          );
        }
      }
    );
  }

  bool tryTransitionToReadNew()
  {
    std::lock_guard<std::mutex> lock(_state_mutex);

    if ((_internal_state != state_flags::READ_AND_WRITE_GENERAL) ||
      (((VOLUME_SWAPS + _write_idx - _read_idx) % VOLUME_SWAPS) != 2))
    {
      // Not ready.
      return false;
    }
    _read_idx = (_read_idx + 1) % VOLUME_SWAPS;
    _internal_state = state_flags::READ_NEW_VOLUME;
    //fprintf(stderr, "transition to READNEW state\n");
    return true;
  }

  bool completeTransitionToReadNew()
  {
    std::lock_guard<std::mutex> lock(_state_mutex);

    if (_internal_state != state_flags::READ_NEW_VOLUME)
    {
      throw std::runtime_error("bad state when completing transitioning to new read");
    }
    _internal_state = state_flags::READ_AND_WRITE_GENERAL;
    //fprintf(stderr, "complete to READNEW state\n");
    return true;
  }

  bool tryTransitionToWriteNew()
  {
    std::lock_guard<std::mutex> lock(_state_mutex);

    if ((_internal_state != state_flags::READ_AND_WRITE_GENERAL) ||
      (((VOLUME_SWAPS + _write_idx - _read_idx) % VOLUME_SWAPS) != 1))
    {
      // Not ready.
      return false;
    }
    int write_idx_tmp = (_write_idx + 1) % VOLUME_SWAPS;
    _write_idx = (_write_idx + 1) % VOLUME_SWAPS;
    _internal_state = state_flags::WRITE_NEW_VOLUME;
    //fprintf(stderr, "transition to WRITENEW state\n");
    return true;
  }

  bool completeTransitionToWriteNew()
  {
    std::lock_guard<std::mutex> lock(_state_mutex);

    if (_internal_state != state_flags::WRITE_NEW_VOLUME)
    {
      throw std::runtime_error("bad state when completing transitioning to new write");
    }
    _internal_state = state_flags::READ_AND_WRITE_GENERAL;
    //fprintf(stderr, "complete to WRITENEW state\n");
    return true;
  }

  int read_idx() const
  {
    std::lock_guard<std::mutex> lock(_state_mutex);
    return _read_idx;
  }

  int write_idx() const
  {
    std::lock_guard<std::mutex> lock(_state_mutex);
    return _write_idx;
  }

  vk::raii::DescriptorSet const &read_descriptor_set() const
  {
    return *(_read_descriptor_set[_read_idx]);
  }

  vk::raii::DescriptorSet const &write_descriptor_set() const
  {
    return *(_write_descriptor_set[_write_idx]);
  }

  vk::raii::DescriptorSetLayout const &write_descriptor_set_layout() const
  {
    return *(_write_descriptor_set_layout);
  }

  vk::raii::DescriptorSetLayout const &read_descriptor_set_layout() const
  {
    return *(_read_descriptor_set_layout);
  }

  enum class state_flags
  {
    READ_AND_WRITE_GENERAL = 0,
    READ_NEW_VOLUME = 1,
    WRITE_NEW_VOLUME = 2
  };

private:
  std::unique_ptr<vk::raii::DescriptorSetLayout> _read_descriptor_set_layout;
  std::array<std::unique_ptr<vk::raii::DescriptorPool>, VOLUME_SWAPS>
    _read_descriptor_pool;
  std::array<std::unique_ptr<vk::raii::DescriptorSet>, VOLUME_SWAPS>
    _read_descriptor_set;
  std::unique_ptr<vk::raii::DescriptorSetLayout> _write_descriptor_set_layout;
  std::array<std::unique_ptr<vk::raii::DescriptorPool>, VOLUME_SWAPS>
    _write_descriptor_pool;
  std::array<std::unique_ptr<vk::raii::DescriptorSet>, VOLUME_SWAPS>
    _write_descriptor_set;
  std::array<std::unique_ptr<Volume>, VOLUME_SWAPS> volumes;

  int _read_idx;
  int _write_idx;

  mutable std::mutex _state_mutex;
  state_flags _internal_state;
};
