#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <mutex>

struct SceneBuffers;

class FrameData
{
public:
  inline FrameData(
    vk::raii::PhysicalDevice const &iphysical_device,
    vk::raii::Device const &idevice,
    vk::raii::Queue const &iqueue,
    std::mutex *iqueue_mutex,
    uint32_t iqueueFamilyIndex)
  : _device(idevice)
  , _queue(iqueue)
  , _queue_mutex(iqueue_mutex)
  , _in_use(false)
  {
    vk::CommandPoolCreateInfo command_pool_info(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      iqueueFamilyIndex
    );

    _command_pool = std::make_unique<vk::raii::CommandPool>(
      idevice, command_pool_info
    );

    vk::CommandBufferAllocateInfo command_buffer_info(
      **_command_pool, vk::CommandBufferLevel::ePrimary, 1
    );

    _command_buffer = std::make_unique<vk::raii::CommandBuffer>(
      std::move(
        vk::raii::CommandBuffers(idevice, command_buffer_info
        ).front()
      )
    );
    
    _fence = std::make_unique<vk::raii::Fence>(
      idevice.createFence(
        vk::FenceCreateInfo(
          vk::FenceCreateFlagBits::eSignaled
        )
      )
    );
    _present_semaphore = std::make_unique<vk::raii::Semaphore>(
      idevice.createSemaphore(vk::SemaphoreCreateInfo())
    );
    _render_semaphore = std::make_unique<vk::raii::Semaphore>(
      idevice.createSemaphore(vk::SemaphoreCreateInfo())
    );
  }

  inline void clearAndBegin()
  {
    _in_use = false;
    _command_buffer->reset({});
    _command_buffer->begin(vk::CommandBufferBeginInfo());
  }

  inline void endAndSubmit()
  {
    _command_buffer->end();
    const vk::PipelineStageFlags wait_destination_stage_mask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
    std::vector<vk::Semaphore> dependency_list = { *_present_semaphore };
    std::vector<vk::PipelineStageFlags> destination_stage_list =
      { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    std::vector<vk::Semaphore> completed_list = { *_render_semaphore };
    std::vector<vk::CommandBuffer> command_buffer = { *_command_buffer };
    vk::SubmitInfo submit_info(
      dependency_list,
      destination_stage_list,
      command_buffer,
      completed_list
    );

    {
      std::lock_guard<std::mutex> lock(*_queue_mutex);
      _queue.submit(submit_info, *_fence);
      _in_use = true;
    }
  }

  inline vk::raii::CommandBuffer const &command_buffer() const
  {
    return *_command_buffer;
  }

  inline vk::raii::Fence const &fence() const
  {
    return *_fence;
  }

  inline bool in_use() const
  {
    return _in_use;
  }

  inline void set_not_in_use()
  {
    _in_use = false;
  }

  inline vk::raii::Semaphore const &render_semaphore() const
  {
    return *_render_semaphore;
  }

  inline vk::raii::Semaphore const &present_semaphore() const
  {
    return *_present_semaphore;
  }

  std::unique_ptr<struct SceneBuffers> scene_buffers;

protected:
  vk::raii::Device const &_device;
  vk::raii::Queue const &_queue;
  std::mutex *_queue_mutex;
  std::unique_ptr<vk::raii::CommandPool> _command_pool;
  std::unique_ptr<vk::raii::CommandBuffer> _command_buffer;
  std::unique_ptr<vk::raii::Fence> _fence;
  std::unique_ptr<vk::raii::Semaphore> _present_semaphore;
  std::unique_ptr<vk::raii::Semaphore> _render_semaphore;
  bool _in_use;
};

#define FRAMES_IN_FLIGHT 2

class FramePool
{
public:
  FramePool(nullptr_t)
    : _framenumber(0)
  {
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
      _frames[i] = nullptr;
  }

  FramePool(
    vk::raii::PhysicalDevice const &iphysical_device,
    vk::raii::Device const &idevice,
    vk::raii::Queue const &iqueue,
    std::mutex *iqueue_mutex,
    uint32_t iqueueFamilyIndex)
    : _framenumber(0)
  {
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
      _frames[i] = std::make_unique<FrameData>(
        iphysical_device,
        idevice,
        iqueue,
        iqueue_mutex,
        iqueueFamilyIndex);
    }
  }

  int count() const
  {
    return FRAMES_IN_FLIGHT;
  }

  FrameData &current_frame()
  {
    return *(_frames[_framenumber % FRAMES_IN_FLIGHT]);
  }

  FrameData *previous_frame()
  {
    if (_framenumber == 0) return nullptr;
    return &(*(_frames[(_framenumber - 1) % FRAMES_IN_FLIGHT]));
  }

  uintmax_t incrementFrameCount()
  {
    return ++_framenumber;
  }

  std::array<std::unique_ptr<FrameData>, FRAMES_IN_FLIGHT> _frames;
  uintmax_t _framenumber;
};
