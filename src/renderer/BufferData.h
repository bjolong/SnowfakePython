#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Memory.h"

template <typename SubmissionFunction>
void oneTimeSubmitAndWaitToComplete(
  vk::raii::Device const &device,
  vk::raii::CommandBuffer const &command_buffer,
  vk::raii::Queue const & queue,
  std::mutex *queue_lock,
  SubmissionFunction const &submission_function)
{
  vk::raii::Fence temporary_fence(device.createFence(vk::FenceCreateInfo()));
  command_buffer.begin(
    vk::CommandBufferBeginInfo(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    )
  );
  submission_function(command_buffer);
  command_buffer.end();
  vk::SubmitInfo submit_info(
    nullptr, nullptr, *command_buffer
  );
  if (queue_lock != nullptr)
  {
    std::lock_guard<std::mutex> lock_guard(*queue_lock);
    queue.submit(submit_info, *temporary_fence);
  } else
  {
    queue.submit(submit_info, *temporary_fence);
  }
  queue.waitIdle();
  command_buffer.reset();
  // Timeout is in nanoseconds.
  while (vk::Result::eTimeout ==
    device.waitForFences( { *temporary_fence }, VK_TRUE, 10000000 ));
}

template <typename SubmissionFunction>
void oneTimeSubmitAndWaitToComplete(
  vk::raii::Device const &device,
  vk::raii::CommandPool const &command_pool,
  vk::raii::Queue const & queue,
  std::mutex *queue_lock,
  SubmissionFunction const &submission_function)
{
  vk::raii::CommandBuffer command_buffer = std::move(
    vk::raii::CommandBuffers(device,
      {*command_pool, vk::CommandBufferLevel::ePrimary, 1 }
    ).front()
  );
  oneTimeSubmitAndWaitToComplete(
    device, command_buffer, queue, queue_lock, submission_function);
}

struct BufferData
{
  BufferData(vk::raii::PhysicalDevice const & physical_device,
    vk::raii::Device const &device, vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags property_flags)
    : _device_memory(nullptr)
    , _buffer(device, vk::BufferCreateInfo({}, size, usage))
    , _size(size)
    , _usage(usage)
    , _property_flags(property_flags)
  {
    _device_memory = allocateDeviceMemory(device,
      physical_device.getMemoryProperties(),
      _buffer.getMemoryRequirements(), property_flags);
    _buffer.bindMemory(_device_memory, 0);
  }

  BufferData(std::nullptr_t)
    : _device_memory(nullptr)
    , _buffer(nullptr)
    , _size(0)
    , _usage(vk::BufferUsageFlags{})
    , _property_flags(vk::MemoryPropertyFlags{})
    {}

  template <typename T>
  void upload(vk::raii::Device const &device,
    T const &data) const
  {
    assert(_property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
    assert(_size >= sizeof(T));

    void *ptr_T = _device_memory.mapMemory(0, sizeof(T));
    memcpy(ptr_T, &data, sizeof(T));
    if (!(_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
      device.flushMappedMemoryRanges(
        vk::MappedMemoryRange(_device_memory, 0, sizeof(T))
      );
    }
    _device_memory.unmapMemory();
  }

  template <typename T>
  void upload(vk::raii::Device const &device,
    T const *data, size_t data_size) const
  {
    assert(_property_flags & vk::MemoryPropertyFlagBits::eHostVisible);

    void *ptr_T = _device_memory.mapMemory(0, data_size);
    memcpy(ptr_T, data, data_size);
    if (!(_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
      device.flushMappedMemoryRanges(
        vk::MappedMemoryRange(_device_memory, 0, data_size)
      );
    }
    _device_memory.unmapMemory();
  }

  template <typename T>
  void upload(vk::raii::Device const &device,
    std::vector<T> const &data) const
  {
    upload(device, data.data(), data.size() * sizeof(T));
  }

  template <typename T>
  void upload(vk::raii::PhysicalDevice const &physical_device,
    vk::raii::Device const &device,
    vk::raii::CommandPool const & command_pool,
    vk::raii::Queue const &queue,
    std::mutex *queue_lock,
    std::vector<T> const &data) const
  {
    assert(_usage & vk::BufferUsageFlagBits::eTransferDst);
    assert(_property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);

    BufferData staging_buffer(
      physical_device, device, data.size() * sizeof(T),
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible);

    void *ptr_T = staging_buffer._device_memory.mapMemory(
      0, data.size() * sizeof(T));
    memcpy(ptr_T, data.data(), data.size() * sizeof(T));
    if (!(_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
      device.flushMappedMemoryRanges(
        vk::MappedMemoryRange(staging_buffer._device_memory, 0,
          data.size() * sizeof(T))
      );
    }
    staging_buffer._device_memory.unmapMemory();

    oneTimeSubmitAndWaitToComplete(device, command_pool, queue, queue_lock,
      [&](vk::raii::CommandBuffer const& command_buffer)
      {
        command_buffer.copyBuffer(*(staging_buffer._buffer), *(this->_buffer),
          vk::BufferCopy(0, 0, data.size() * sizeof(T))
        );
      });
  }

  vk::raii::DeviceMemory _device_memory;
  vk::raii::Buffer _buffer;

  vk::DeviceSize _size;
  vk::BufferUsageFlags _usage;
  vk::MemoryPropertyFlags _property_flags;
};
