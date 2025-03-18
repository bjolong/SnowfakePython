#pragma once

#include <cstring>
#include <memory>
#include <vector>
#include <tuple>
#include <iostream>
#include <mutex>

#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vkComputeHelper
{
  class Context;
  class Schema;
  class Program;
  class TensorParameterSet;
  class Step;

  template <uint32_t BUFFER_USAGE_FLAGS, uint32_t MEMORY_PROPERTY_FLAGS>
  class LinearMemoryPool
  {
  friend class Context;

  public:
    inline LinearMemoryPool(const LinearMemoryPool &other) = delete;
    inline LinearMemoryPool &operator=(const LinearMemoryPool &other) = delete;

    inline LinearMemoryPool(LinearMemoryPool &&other)
    : _physical_device(other._physical_device)
    , _device(other._device)
    , _backing_device_memory(std::move(other._backing_device_memory))
    , _in_test_run_mode(std::move(other._in_test_run_mode))
    , _byte_ptr(std::move(other._byte_ptr))
    , _allocated_memory_size(std::move(other._allocated_memory_size))
    , _mapped_memory_ptr(std::move(other._mapped_memory_ptr))
    {
      other._backing_device_memory = nullptr;
      other._in_test_run_mode = true;
      other._byte_ptr = 0;
      other._allocated_memory_size = 0;
      other._mapped_memory_ptr = nullptr;
    }

    inline LinearMemoryPool &operator=(LinearMemoryPool &&other)
    {
      if (this != &other)
      {
        if ((MEMORY_PROPERTY_FLAGS ==
          static_cast<uint32_t>(
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
          )) && (_mapped_memory_ptr != nullptr))
        {
          _backing_device_memory->unmapMemory();
          _mapped_memory_ptr = nullptr;
        }

        std::weak_ptr<vk::raii::DeviceMemory> weak_device_memory_ptr =
          _backing_device_memory;
        _backing_device_memory = nullptr;
        if (!weak_device_memory_ptr.expired())
        {
          fprintf(stderr, "problem with linear memory pool, device memory may "
            "not have been released.\n");
        }

        _physical_device = other._physical_device;
        _device = other._device;
        _backing_device_memory = std::move(other._backing_device_memory);
        _in_test_run_mode = std::move(other._in_test_run_mode);
        _byte_ptr = std::move(other._byte_ptr);
        _allocated_memory_size = std::move(other._allocated_memory_size);
        _mapped_memory_ptr = std::move(other._mapped_memory_ptr);
        other._backing_device_memory = nullptr;
        other._in_test_run_mode = true;
        other._byte_ptr = 0;
        other._allocated_memory_size = 0;
        other._mapped_memory_ptr = nullptr;
      }
      return *this;
    }

    inline void dryrunAllocate(uintmax_t size_bytes)
    {
      if (_in_test_run_mode)
      {
        // Make 32-byte aligned.
        const uintmax_t amount_bytes =
          (size_bytes == 0) ? 0 :
            ((((size_bytes - 1) >> 5) + 1) << 5);
        _byte_ptr += amount_bytes;
      } else // (_in_test_run_mode)
      {
        throw std::runtime_error("cannot use dry run allocation "
          "while real memory pool is active");
        return;
      } // else (_in_test_run_mode)
    }

    inline void allocatePool()
    {
      if (!_in_test_run_mode)
      {
        throw std::runtime_error("cannot allocate the memory pool "
          "while real memory pool is active");
        return;
      }

      if (_byte_ptr == 0)
      {
        throw std::runtime_error("cannot allocate zero-size pool - "
          "have you dry run your intended allocations yet?");
      }

      //_byte_ptr = 100000000;

      vk::BufferCreateInfo buffer_create_info(
        {}, _byte_ptr,
        static_cast<vk::BufferUsageFlags>(BUFFER_USAGE_FLAGS)
      );

      vk::raii::Buffer test_buffer(*_device, buffer_create_info);

      vk::PhysicalDeviceMemoryProperties memory_properties =
        _physical_device->getMemoryProperties();
      vk::MemoryRequirements memory_requirements =
        test_buffer.getMemoryRequirements();
      vk::MemoryPropertyFlags required_memory_properties =
        static_cast<vk::MemoryPropertyFlags>(
          MEMORY_PROPERTY_FLAGS
        );
//        vk::MemoryPropertyFlagBits::eDeviceLocal;

      uint32_t memory_type_index = -1;
      for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
      {
        if (memory_requirements.memoryTypeBits & (1 << i))
        {
          if (((memory_properties.memoryTypes[i]).propertyFlags &
            required_memory_properties) == required_memory_properties)
          {
            memory_type_index = i;
            break;
          }
        } // (memory_requirements.memoryTypeBits & (1 << i))
      } // i

      if (memory_type_index == uint32_t(-1))
      {
        throw std::runtime_error("could not find suitable memory type for "
          "memory pool creation");
        return;
      }

      vk::MemoryAllocateInfo memory_allocation_info(
        memory_requirements.size, memory_type_index
      );

      _backing_device_memory =
        std::make_shared<vk::raii::DeviceMemory>(
          *_device, memory_allocation_info);

      // Don't bind memory, this is merely for demo purposes.
      _in_test_run_mode = false;
      _allocated_memory_size = memory_requirements.size;
      _byte_ptr = 0;

      if (MEMORY_PROPERTY_FLAGS ==
        static_cast<uint32_t>(
          vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent
        ))
      {
        _mapped_memory_ptr =
          _backing_device_memory->mapMemory(0, _allocated_memory_size);
      }
    }

    inline std::pair<std::shared_ptr<vk::raii::DeviceMemory>, uintmax_t>
      allocate(uintmax_t size_bytes)
    {
      if (_in_test_run_mode)
      {
        throw std::runtime_error("cannot allocate from the memory pool "
          "while pool is in dry run mode");
      }

      const uintmax_t begin_byte_ptr = _byte_ptr;

      // Make 32-byte aligned.
      const uintmax_t amount_bytes =
        (size_bytes == 0) ? 0 :
          ((((size_bytes - 1) >> 5) + 1) << 5);
      _byte_ptr += amount_bytes;

      if (_byte_ptr > _allocated_memory_size)
      {
        throw std::runtime_error("device memory pool exhausted");
      }

      return std::make_pair<std::shared_ptr<vk::raii::DeviceMemory>, uintmax_t>(
        std::shared_ptr<vk::raii::DeviceMemory>(_backing_device_memory),
        uintmax_t(begin_byte_ptr)
      );
    }

    inline bool inTestMode() const
    {
      return _in_test_run_mode;
    }

    inline void *getOffsetPointer(uintmax_t offset_bytes)
    {
      return
        &(reinterpret_cast<uint8_t *>(_mapped_memory_ptr)[offset_bytes]);
    }

    inline ~LinearMemoryPool()
    {
      if (_backing_device_memory != nullptr)
      {
        if ((MEMORY_PROPERTY_FLAGS ==
          static_cast<uint32_t>(
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
          )) && (_mapped_memory_ptr != nullptr))
        {
          _backing_device_memory->unmapMemory();
          _mapped_memory_ptr = nullptr;
        }

        std::weak_ptr<vk::raii::DeviceMemory> weak_device_memory_ptr =
          _backing_device_memory;
        _backing_device_memory = nullptr;
        if (!weak_device_memory_ptr.expired())
        {
          fprintf(stderr, "problem with linear memory pool, device memory may "
            "not have been released.\n");
        }
      }
    }

  protected:
    inline LinearMemoryPool(
      vk::raii::PhysicalDevice const &iphysical_device,
      vk::raii::Device const &idevice)
    : _physical_device(&iphysical_device)
    , _device(&idevice)
    , _backing_device_memory(nullptr)
    , _in_test_run_mode(true)
    , _byte_ptr(0)
    , _allocated_memory_size(0)
    , _mapped_memory_ptr(nullptr)
    {}

  private:
    vk::raii::PhysicalDevice const *_physical_device;
    vk::raii::Device const *_device;

    std::shared_ptr<vk::raii::DeviceMemory> _backing_device_memory;
    bool _in_test_run_mode;
    uintmax_t _byte_ptr;
    uintmax_t _allocated_memory_size;

    void *_mapped_memory_ptr;
  };

  typedef LinearMemoryPool<
    static_cast<uint32_t>(
      vk::BufferUsageFlagBits::eStorageBuffer |
      vk::BufferUsageFlagBits::eTransferSrc |
      vk::BufferUsageFlagBits::eTransferDst),
    static_cast<uint32_t>(
      vk::MemoryPropertyFlagBits::eDeviceLocal
    )> LinearDeviceMemoryPool;

  typedef LinearMemoryPool<
    static_cast<uint32_t>(
      vk::BufferUsageFlagBits::eTransferSrc |
      vk::BufferUsageFlagBits::eTransferDst),
    static_cast<uint32_t>(
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent      
    )> LinearStagingMemoryPool;

  class Tensor
  {
    friend class Context;
    friend class UploadTensors;
    friend class DownloadTensors;
  public:
    inline Tensor(
      vk::raii::PhysicalDevice const &iphysical_device,
      vk::raii::Device const &idevice,
      LinearDeviceMemoryPool &ildmp,
      std::size_t size_bytes,
      bool is_shared)
      : _physical_device(&iphysical_device)
      , _device(&idevice)
      , _tensor_size(size_bytes)
      , _device_memory(nullptr)
    {
      vk::BufferCreateInfo buffer_create_info(
        {}, _tensor_size,
        (is_shared) ?
          (vk::BufferUsageFlagBits::eStorageBuffer |
           vk::BufferUsageFlagBits::eTransferSrc |
           vk::BufferUsageFlagBits::eTransferDst) :
          vk::BufferUsageFlagBits::eStorageBuffer
      );

      _buffer =
        std::make_unique<vk::raii::Buffer>(
          *_device, buffer_create_info);

      if (ildmp.inTestMode())
        ildmp.allocatePool();

      std::pair<std::shared_ptr<vk::raii::DeviceMemory>, uintmax_t>
        allocation = ildmp.allocate(_tensor_size);

      _device_memory = allocation.first;
      _device_memory_offset = allocation.second;

      _buffer->bindMemory(**_device_memory, _device_memory_offset);
    }

    inline Tensor(const Tensor &other) = delete;
    inline Tensor &operator=(const Tensor &other) = delete;

    inline vk::raii::PhysicalDevice const &physical_device() const
    {
      return *_physical_device;
    }

    inline vk::raii::Device const &device() const
    {
      return *_device;
    }

    inline vk::raii::Buffer const &buffer() const
    {
      return *_buffer;
    }

    inline size_t size() const
    {
      return _tensor_size;
    }

    inline Tensor(Tensor &&other)
    : _physical_device(std::move(other._physical_device))
    , _device(std::move(other._device))
    , _tensor_size(std::move(other._tensor_size))
    , _device_memory(std::move(other._device_memory))
    , _device_memory_offset(std::move(other._device_memory_offset))
    , _buffer(std::move(other._buffer))
    {
      other._buffer = nullptr;
      other._device_memory_offset = -1;
      other._device_memory = nullptr;
    }

    inline Tensor &operator=(Tensor &&other)
    {
      if (this != &other)
      {
        _physical_device = std::move(other._physical_device);
        _device = std::move(other._device);
        _tensor_size = std::move(other._tensor_size);
        _device_memory = std::move(other._device_memory);
        _device_memory_offset = std::move(other._device_memory_offset);
        _buffer = std::move(other._buffer);
        other._buffer = nullptr;
        other._device_memory_offset = -1;
        other._device_memory = nullptr;
      }
      return *this;
    }

    inline virtual ~Tensor()
    {
      if ((_buffer != nullptr) || (_device_memory != nullptr))
      {
        _buffer = nullptr;
        _device_memory = nullptr;
      }
    }

  protected:
    inline virtual vk::raii::Buffer const *getStagingBuffer() const = 0;

    vk::raii::PhysicalDevice const *_physical_device;
    vk::raii::Device const *_device;
    size_t _tensor_size;
    std::shared_ptr<vk::raii::DeviceMemory> _device_memory;
    uintmax_t _device_memory_offset;
    std::unique_ptr<vk::raii::Buffer> _buffer;
  };

  template <typename T>
  class SharedTensor : public Tensor
  {
    friend class Context;
  protected:
    inline SharedTensor(
      vk::raii::PhysicalDevice const &iphysical_device,
      vk::raii::Device const &idevice,
      LinearStagingMemoryPool &ilsmp,
      LinearDeviceMemoryPool &ildmp,
      std::size_t ielement_count)
      : Tensor(iphysical_device, idevice, ildmp, ielement_count * sizeof(T), true)
      , _staging_device_memory(nullptr)
      , _mapped_data(nullptr)
    {
      vk::BufferCreateInfo buffer_create_info(
        {}, _tensor_size,
        vk::BufferUsageFlagBits::eTransferSrc |
        vk::BufferUsageFlagBits::eTransferDst
      );

      _staging_buffer =
        std::make_unique<vk::raii::Buffer>(
          device(), buffer_create_info);

      if (ilsmp.inTestMode())
        ilsmp.allocatePool();

      std::pair<std::shared_ptr<vk::raii::DeviceMemory>, uintmax_t>
        allocation = ilsmp.allocate(_tensor_size);

      _staging_device_memory = allocation.first;
      _staging_device_memory_offset = allocation.second;

      _staging_buffer->bindMemory(
        **_staging_device_memory, _staging_device_memory_offset);

      _mapped_data = reinterpret_cast<float *>(
        ilsmp.getOffsetPointer(_staging_device_memory_offset));
    }

    inline virtual vk::raii::Buffer const *getStagingBuffer() const
    {
      return _staging_buffer.get();
    }

  public:
    inline SharedTensor(SharedTensor &&other)
    : Tensor(std::move(other))
    , _staging_device_memory(std::move(other._staging_device_memory))
    , _staging_buffer(std::move(other._staging_buffer))
    , _mapped_data(std::move(other._mapped_data))
    {
      other._mapped_data = nullptr;
      other._staging_buffer = nullptr;
      other._staging_device_memory = nullptr;
    }

    inline SharedTensor &operator=(SharedTensor &&other)
    {
      if (this != &other)
      {
        Tensor::operator=(std::move(other));
        _staging_device_memory = std::move(other._staging_device_memory);
        _staging_buffer = std::move(other._staging_buffer);
        _mapped_data = other._mapped_data;
        other._mapped_data = nullptr;
        other._staging_buffer = nullptr;
        other._staging_device_memory = nullptr;
      }
      return *this;
    }

    inline T *data() { return _mapped_data; }
    inline T const *data() const { return _mapped_data; }

    inline virtual ~SharedTensor()
    {
      if (_mapped_data != nullptr)
      {
        _mapped_data = nullptr;
      }
    }
    
  protected:
    std::shared_ptr<vk::raii::DeviceMemory> _staging_device_memory;
    uintmax_t _staging_device_memory_offset;
    std::unique_ptr<vk::raii::Buffer> _staging_buffer;
    T *_mapped_data;
  };

  class StorageTensor : public Tensor
  {
    friend class Context;
  protected:
    inline StorageTensor(
      vk::raii::PhysicalDevice const &iphysical_device,
      vk::raii::Device const &idevice,
      LinearDeviceMemoryPool &ildmp,
      std::size_t isize_bytes)
      : Tensor(iphysical_device, idevice, ildmp, isize_bytes, false)
    {}

    inline virtual vk::raii::Buffer const *getStagingBuffer() const
    {
      return nullptr;
    }

  public:
    inline StorageTensor(StorageTensor &&other)
    : Tensor(std::move(other))
    {
    }

    inline StorageTensor &operator=(StorageTensor &&other)
    {
      if (this != &other)
      {
        Tensor::operator=(std::move(other));
      }
      return *this;
    }

    inline virtual ~StorageTensor()
    {
    }

  };

  class TensorParameterSet
  {
    friend class Context;
    friend class Work;
    friend class Program;
  protected:
    inline TensorParameterSet(
      vk::raii::Device const &idevice,
      const std::vector<std::shared_ptr<Tensor> > &tensors)
      : _device(idevice)
      , _tensors(tensors)
    {
      std::vector<vk::DescriptorSetLayoutBinding> bindings(tensors.size());
      for (size_t i = 0; i < bindings.size(); i++)
      {
        bindings[i] = vk::DescriptorSetLayoutBinding(
          static_cast<uint32_t>(i),
          vk::DescriptorType::eStorageBuffer,
          1,
          vk::ShaderStageFlagBits::eCompute);

      } // i

      vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info(
        vk::DescriptorSetLayoutCreateFlags(), bindings);
      
      _descriptor_set_layout = std::make_unique<vk::raii::DescriptorSetLayout>(
        _device, descriptor_set_layout_info
      );

      vk::DescriptorPoolSize descriptor_pool_size(
        vk::DescriptorType::eStorageBuffer,
        static_cast<uint32_t>(tensors.size())
      );

      vk::DescriptorPoolCreateInfo descriptor_pool_info(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1, 1,
        &descriptor_pool_size
      );

      _descriptor_pool = std::make_unique<vk::raii::DescriptorPool>(
        idevice, descriptor_pool_info
      );

      vk::DescriptorSetAllocateInfo descriptor_set_allocate_info(
        **_descriptor_pool, **_descriptor_set_layout
      );

      _descriptor_set = std::make_unique<vk::raii::DescriptorSet>(
        std::move(
          vk::raii::DescriptorSets(
            _device,
            descriptor_set_allocate_info).front()
        )
      );

      for (size_t i = 0; i < tensors.size(); i++)
      {
        vk::DescriptorBufferInfo descriptor_buffer_info(
          *(tensors[i]->buffer()),
          0,
          tensors[i]->size()
        );

        vk::WriteDescriptorSet write_descriptor_set(
          **_descriptor_set,
          i, // shader bind location
          0, 1, vk::DescriptorType::eStorageBuffer,
          nullptr,
          &descriptor_buffer_info);

        _device.updateDescriptorSets(write_descriptor_set, nullptr);
      }
    }

    vk::raii::DescriptorSetLayout const &descriptor_set_layout() const
    {
      return *_descriptor_set_layout;
    }

    size_t size() const
    {
      return _tensors.size();
    }

    vk::raii::Device const &_device;
    std::vector<std::shared_ptr<Tensor> > _tensors;
    std::unique_ptr<vk::raii::DescriptorSetLayout> _descriptor_set_layout;
    std::unique_ptr<vk::raii::DescriptorPool> _descriptor_pool;
    std::unique_ptr<vk::raii::DescriptorSet> _descriptor_set;
  };

  class ConstantBase
  {
    friend class Program;
    friend class Work;
  protected:
    inline ConstantBase()
     : _stored_size(0)
    {}

    std::vector<unsigned char> _stored_data;
    size_t _stored_size;
  
  public:
    inline void const *data() const
    {
      return reinterpret_cast<void const *>(_stored_data.data());
    }

    inline size_t size() const
    {
      return _stored_size;
    }
  };

  template <typename T>
  class Constant : public ConstantBase
  {
  public:
    inline Constant(T ivalue)
     : ConstantBase()
    {
      _stored_data = std::vector<unsigned char>(sizeof(T));
      _stored_size = sizeof(T);
      reinterpret_cast<T *>(_stored_data.data())[0] = ivalue;
    }
  };

  class Program
  {
    friend class Context;
    friend class Work;
  protected:
    inline Program(
      vk::raii::Device const &idevice,
      std::vector<ConstantBase> const &spec_consts,
      std::vector<ConstantBase> const &example_push_consts,
      TensorParameterSet const &example_tensor_parameter_set,
      std::vector<uint32_t> const &SPIRV,
      vk::raii::DescriptorSetLayout const *extra_descriptor_set = nullptr)
      : _device(idevice)
    {
      std::vector<vk::SpecializationMapEntry> specialization_entries;
      size_t size_total = 0;
      for (size_t i = 0; i < spec_consts.size(); i++)
      {
        const size_t this_element_size = spec_consts[i].size();
        vk::SpecializationMapEntry specialization_map_entry(
          static_cast<uint32_t>(i),
          static_cast<uint32_t>(size_total),
          static_cast<uint32_t>(this_element_size)
        );

        size_total += this_element_size;
        specialization_entries.push_back(specialization_map_entry);

      } // i

      std::unique_ptr<unsigned char[]> specialisation_constants_data =
        std::make_unique<unsigned char[]>(size_total);
      
      size_total = 0;
      for (size_t i = 0; i < spec_consts.size(); i++)
      {
        const size_t this_element_size = spec_consts[i].size();
        std::memcpy(
          &((specialisation_constants_data.get())[size_total]),
          spec_consts[i].data(),
          this_element_size
        );
        size_total += this_element_size;

      } // i

      vk::SpecializationInfo specialization_info(
        static_cast<uint32_t>(specialization_entries.size()),
        specialization_entries.data(),
        static_cast<uint32_t>(size_total),
        specialisation_constants_data.get()
      );

      size_total = 0;
      for (size_t i = 0; i < example_push_consts.size(); i++)
      {
        const size_t this_element_size = example_push_consts[i].size();
        size_total += this_element_size;
      }

      // TODO FIXME - this needs to be a multiple of 4 - 32-bit padded.
      vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eCompute, 0, static_cast<uint32_t>(size_total)
      );

      _shader_module = std::make_unique<vk::raii::ShaderModule>(
        _device, vk::ShaderModuleCreateInfo(
          vk::ShaderModuleCreateFlags(), SPIRV
        )
      );

      std::vector<vk::DescriptorSetLayout> v_descriptor_set_layouts;
      if (example_tensor_parameter_set.size() > 0)
        v_descriptor_set_layouts.push_back(
          *(example_tensor_parameter_set.descriptor_set_layout())
        );
      if (extra_descriptor_set != nullptr)
        v_descriptor_set_layouts.push_back(
          **extra_descriptor_set
        );
      std::vector<vk::PushConstantRange> v_push_constant_range;
      if (example_push_consts.size() > 0)
        v_push_constant_range.push_back(
          push_constant_range
        );

      vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(),
        v_descriptor_set_layouts,
        v_push_constant_range
      );

      _pipeline_layout = std::make_unique<vk::raii::PipelineLayout>(
        _device, pipeline_layout_info);

      vk::PipelineShaderStageCreateInfo shader_stage_info(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eCompute,
        **_shader_module,
        "main",
        &specialization_info);
      
      vk::ComputePipelineCreateInfo compute_pipeline_info(
        vk::PipelineCreateFlags(), 
        shader_stage_info,
        **_pipeline_layout,
        vk::Pipeline(),
        0);

      vk::PipelineCacheCreateInfo pipeline_cache_info;
      vk::raii::PipelineCache temporary_cache(_device, pipeline_cache_info);

      _pipeline = std::make_unique<vk::raii::Pipeline>(
        _device.createComputePipeline(temporary_cache, compute_pipeline_info)
      );
    }

    vk::raii::Device const &_device;
    std::unique_ptr<vk::raii::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vk::raii::ShaderModule> _shader_module;
    std::unique_ptr<vk::raii::Pipeline> _pipeline;
  };

  class Step
  {
  public:
    inline virtual ~Step() {}
    virtual void recordCommands(vk::raii::CommandBuffer const &command_buffer) = 0;
  };

  class UploadTensors : public Step
  {
  public:
    UploadTensors(std::vector<std::shared_ptr<Tensor> > const &tensors)
      : temp_tensors(tensors)
    {}

    void recordCommands(vk::raii::CommandBuffer const &command_buffer)
    {
      for (size_t i = 0; i < temp_tensors.size(); i++)
      {
        vk::BufferCopy buffer_copy(0, 0, temp_tensors[i]->size());
        vk::raii::Buffer const *staging = temp_tensors[i]->getStagingBuffer();
        command_buffer.copyBuffer(
          **staging,
          **(temp_tensors[i]->_buffer),
          buffer_copy
        );

      } // i
    }
  
    std::vector<std::shared_ptr<Tensor> > const &temp_tensors;
  };

  class DownloadTensors : public Step
  {
  public:
    DownloadTensors(std::vector<std::shared_ptr<Tensor> > const &tensors)
      : temp_tensors(tensors)
    {}

    void recordCommands(vk::raii::CommandBuffer const &command_buffer)
    {
      for (size_t i = 0; i < temp_tensors.size(); i++)
      {
        vk::BufferCopy buffer_copy(0, 0, temp_tensors[i]->size());
        vk::raii::Buffer const *staging = temp_tensors[i]->getStagingBuffer();
        command_buffer.copyBuffer(
          **(temp_tensors[i]->_buffer),
          **staging,
          buffer_copy
        );

      } // i
    }
  
    std::vector<std::shared_ptr<Tensor> > const &temp_tensors;
  };

  class Work : public Step
  {
  public:
    Work(
      std::tuple<unsigned int, unsigned int, unsigned int> workgroups,
      std::vector<ConstantBase> const &push_consts,
      std::shared_ptr<TensorParameterSet> const &parameters,
      std::shared_ptr<Program> const &program,
      vk::raii::DescriptorSet const *extra_descriptor_set = nullptr)
    : _workgroups(workgroups)
    , _push_consts(push_consts)
    , _parameters(parameters)
    , _program(program)
    , _extra_descriptor_set(extra_descriptor_set)
    {}

    void recordCommands(vk::raii::CommandBuffer const &command_buffer)
    {
      if (_push_consts.size() > 0)
      {
        size_t size_total = 0;
        for (int i = 0; i < _push_consts.size(); i++)
          size_total += _push_consts[i].size();

        std::unique_ptr<unsigned char[]> push_constants_data =
          std::make_unique<unsigned char[]>(size_total);

        size_total = 0;
        for (size_t i = 0; i < _push_consts.size(); i++)
        {
          const size_t this_element_size = _push_consts[i].size();
          std::memcpy(
            &((push_constants_data.get())[size_total]),
            _push_consts[i].data(),
            this_element_size
          );
          size_total += this_element_size;
        } // i

        vkCmdPushConstants(
          *command_buffer,
          **(_program->_pipeline_layout),
          static_cast<VkShaderStageFlags>(vk::ShaderStageFlagBits::eCompute),
          0, size_total,
          static_cast<void const *>(push_constants_data.get()) // random types
        );
      }

      command_buffer.bindPipeline(
        vk::PipelineBindPoint::eCompute,
        **(_program->_pipeline));

      if (_extra_descriptor_set == nullptr)
      {
        command_buffer.bindDescriptorSets(
          vk::PipelineBindPoint::eCompute,
          **(_program->_pipeline_layout),
          0, **(_parameters->_descriptor_set),
          nullptr);
      } else
      {
        std::vector<vk::DescriptorSet> these_descriptor_sets = {
          **(_parameters->_descriptor_set),
          **(_extra_descriptor_set)
        };
        command_buffer.bindDescriptorSets(
          vk::PipelineBindPoint::eCompute,
          **(_program->_pipeline_layout),
          0, these_descriptor_sets,
          nullptr);
      }

      command_buffer.dispatch(
        (std::get<0>(_workgroups) > 0) ? std::get<0>(_workgroups) : 1,
        (std::get<1>(_workgroups) > 0) ? std::get<1>(_workgroups) : 1,
        (std::get<2>(_workgroups) > 0) ? std::get<2>(_workgroups) : 1
      );
    }

    std::tuple<unsigned int, unsigned int, unsigned int> _workgroups;
    std::vector<ConstantBase> const &_push_consts;
    std::shared_ptr<TensorParameterSet> _parameters;
    std::shared_ptr<Program> _program;
    vk::raii::DescriptorSet const *_extra_descriptor_set;
  };

  class Schema : public std::enable_shared_from_this<Schema>
  {
    friend class Context;
  protected:
    inline Schema(
      vk::raii::PhysicalDevice const &iphysical_device,
      vk::raii::Device const &idevice,
      vk::raii::Queue const &iqueue,
      std::mutex &iqueue_mutex,
      uint32_t iqueueFamilyIndex)
    : _device(idevice)
    , _queue(iqueue)
    , _queue_mutex(iqueue_mutex)
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
        idevice.createFence(vk::FenceCreateInfo())
      );
      _semaphore = std::make_unique<vk::raii::Semaphore>(
        idevice.createSemaphore(vk::SemaphoreCreateInfo())
      );
    }

  public:
    inline std::shared_ptr<Schema> make()
    {
      _command_buffer->reset({});
      _command_buffer->begin(vk::CommandBufferBeginInfo());
      for (size_t i = 0; i < _steps.size(); i++)
      {
        _steps[i]->recordCommands(*_command_buffer);
      }
      _command_buffer->end();

      return shared_from_this();
    }

    inline std::shared_ptr<Schema> submitForAfter(
      const std::shared_ptr<Schema> &dependency)
    {
      std::vector<vk::PipelineStageFlags> destinations;
      if (dependency != nullptr)
        destinations.push_back(vk::PipelineStageFlagBits::eComputeShader);
      std::vector<vk::Semaphore> semaphores;
      if (dependency != nullptr)
        semaphores.push_back(**(dependency->_semaphore));
      vk::SubmitInfo submit_info(
        semaphores,
        destinations,
        **_command_buffer,
        **_semaphore);

      _device.resetFences(**_fence);

      {
        std::lock_guard<std::mutex> guard(_queue_mutex);
        _queue.submit(submit_info, **_fence);
      }

      return shared_from_this();
    }

    inline std::shared_ptr<Schema> submit()
    {
      return submitForAfter(nullptr);
    }

    inline std::shared_ptr<Schema> waitForCompletion()
    {
      // Timeout is in nanoseconds.
      while (vk::Result::eTimeout ==
        _device.waitForFences( { **_fence }, VK_TRUE, 10000000 ));

      return shared_from_this();
    }

    inline std::shared_ptr<Schema> clear()
    {
      _steps.clear();

      return shared_from_this();
    }

    inline std::shared_ptr<Schema> add(const std::shared_ptr<Step> &step)
    {
      _steps.push_back(step);

      return shared_from_this();
    }

    template <typename T, typename... TArgs>
    inline std::shared_ptr<Schema> add(
      TArgs&&... params)
    {
      std::shared_ptr<T> step( std::make_shared<T>(std::forward<TArgs>(params)...) );
      return this->add(step);
    }

  protected:
    vk::raii::Device const &_device;
    vk::raii::Queue const &_queue;
    std::mutex &_queue_mutex;
    std::unique_ptr<vk::raii::CommandPool> _command_pool;
    std::unique_ptr<vk::raii::CommandBuffer> _command_buffer;
    std::unique_ptr<vk::raii::Fence> _fence;
    std::unique_ptr<vk::raii::Semaphore> _semaphore;
    std::vector<std::shared_ptr<Step> > _steps;
  };

  class Context
  {
  private:
    inline Context(
      std::vector<std::string> (*instance_extension_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::ExtensionProperties> &available,
        void *user_pointer),
      void *instance_extension_choice_cb__user_pointer,
      std::vector<std::string> (*instance_layer_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::LayerProperties> &available,
        void *user_pointer),
      void *instance_layer_choice_cb__user_pointer,
      uint32_t (*physical_device_choice_cb)(
        vk::raii::Instance const &instance,
        std::vector<vk::raii::PhysicalDevice> &physical_device_options,
        void *user_pointer),
      void *physical_device_choice_cb__user_pointer,
      std::vector<std::pair<uint32_t, uint32_t> > (*queue_family_index_choice_cb)(
        vk::raii::PhysicalDevice const &physical_device,
        std::vector<vk::QueueFamilyProperties> &queue_family_properties,
        std::pair<uint32_t, uint32_t> &chosen_compute_index,
        void *user_pointer),
      void *queue_family_index_choice_cb__user_pointer,
      std::vector<std::string> (*device_extension_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::ExtensionProperties> &available,
        void *user_pointer),
      void *device_extension_choice_cb__user_pointer)
    {
      _context = std::make_unique<vk::raii::Context>();

      bool needs_portability_requirements = false;

      const char *portability_required_extensions[] = {
        "VK_KHR_portability_enumeration",
        "VK_KHR_get_physical_device_properties2",
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
      };

      std::vector<std::string> extension_names_to_propose;
      std::vector<vk::ExtensionProperties> available_extensions =
        vk::enumerateInstanceExtensionProperties();
      for (int i = 0; i < available_extensions.size(); i++)
      {
        for (int j = 0; j < (sizeof(portability_required_extensions) / 
          sizeof(portability_required_extensions[0])); j++)
        {
          if (std::strcmp(available_extensions[i].extensionName,
            portability_required_extensions[j]) == 0)
          {
            extension_names_to_propose.push_back(portability_required_extensions[j]);
            needs_portability_requirements = true;
          }
        } // j
      } // i
      std::vector<std::string> extension_names_chosen;
      if (instance_extension_choice_cb != nullptr)
      {
        extension_names_chosen =
          (*instance_extension_choice_cb)(
            extension_names_to_propose,
            available_extensions,
            instance_extension_choice_cb__user_pointer);
      } else // (instance_extension_choice_cb != nullptr)
      {
        extension_names_chosen =
          extension_names_to_propose;
      } // else (instance_extension_choice_cb != nullptr)
      std::vector<const char *> extension_names_to_use;
      for (int l = 0; l < extension_names_chosen.size(); l++)
      {
        extension_names_to_use.push_back(
          extension_names_chosen.at(l).c_str()
        );
      }

      const char *layer_names_to_look_for[] = {
        "VK_LAYER_LUNARG_standard_validation",
        "VK_LAYER_KHRONOS_validation"
      };

      std::vector<std::string> layer_names_to_propose;
      std::vector<vk::LayerProperties> available_layers =
        vk::enumerateInstanceLayerProperties();
      for (int i = 0; i < available_layers.size(); i++)
      {
        for (int j = 0; j < (sizeof(layer_names_to_look_for) /
          sizeof(layer_names_to_look_for[0])); j++)
        {
          if (std::strcmp(available_layers[i].layerName,
            layer_names_to_look_for[j]) == 0)
          {
            layer_names_to_propose.push_back(layer_names_to_look_for[j]);
          }
        } // j
      } // i
      std::vector<std::string> layer_names_chosen;
      if (instance_layer_choice_cb != nullptr)
      {
        layer_names_chosen =
          (*instance_layer_choice_cb)(
            layer_names_to_propose,
            available_layers,
            instance_layer_choice_cb__user_pointer);
      } else
      {
        layer_names_chosen =
          layer_names_to_propose;
      }
      std::vector<const char *> layer_names_to_use;
      for (int l = 0; l < layer_names_chosen.size(); l++)
      {
        layer_names_to_use.push_back(
          layer_names_chosen.at(l).c_str()
        );
      }

#if !defined(BUILD_PYTHON_BINDINGS)
      if (extension_names_to_use.size() > 0)
      {
        fprintf(stderr, "Using extensions:\n");
        for (int i = 0; i < extension_names_to_use.size(); i++)
        {
          fprintf(stderr, "  %s\n", extension_names_to_use[i]);
        }
      }
      if (layer_names_to_use.size() > 0)
      {
        fprintf(stderr, "Using layers:\n");
        for (int i = 0; i < layer_names_to_use.size(); i++)
        {
          fprintf(stderr, "  %s\n", layer_names_to_use[i]);
        }
      }
#endif // !defined(BUILD_PYTHON_BINDINGS)

      vk::ApplicationInfo application_info(
        "Riemann solver 3D",
        1,
        "none",
        1,
        VK_API_VERSION_1_1
      );

      vk::InstanceCreateInfo instance_create_info(
        (needs_portability_requirements) ?
          static_cast<vk::InstanceCreateFlagBits>(0x1) :
          vk::InstanceCreateFlagBits(),
        &application_info,
        layer_names_to_use.size(), layer_names_to_use.data(),
        extension_names_to_use.size(), extension_names_to_use.data()
      );

      try
      {
        _instance = std::make_unique<vk::raii::Instance>(
          *_context, instance_create_info
        );
      }
      catch (vk::SystemError &err)
      {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit(-1);
      }

      std::vector<vk::raii::PhysicalDevice> physical_device_options =
        _instance->enumeratePhysicalDevices();
      if (physical_device_options.empty())
      {
        throw std::runtime_error("No Vulkan physical devices available.");
      }

      // Select the first physical device...
      _physical_device_index = 0;
      // ...unless we have a callback to evaluate the devices.
      if (physical_device_choice_cb != nullptr)
      {
        _physical_device_index = (*physical_device_choice_cb)(
          *_instance,
          physical_device_options, physical_device_choice_cb__user_pointer);
      }

      _physical_device = std::make_unique<vk::raii::PhysicalDevice>(
        std::move(physical_device_options[_physical_device_index])
      );

      std::vector<vk::QueueFamilyProperties> queue_family_properties =
        _physical_device->getQueueFamilyProperties();

      std::pair<uint32_t, uint32_t> chosen_compute_index =
        std::pair<uint32_t, uint32_t>(-1, -1);
      std::pair<uint32_t, uint32_t> chosen_graphics_index =
        std::pair<uint32_t, uint32_t>(-1, -1);
      std::pair<uint32_t, uint32_t> chosen_present_index =
        std::pair<uint32_t, uint32_t>(-1, -1);

      for (std::vector<vk::QueueFamilyProperties>::size_type
        i = 0;
        i < queue_family_properties.size();
        i++)
      {
        if ((queue_family_properties.at(i).queueCount > 0) &&
            (queue_family_properties.at(i).queueFlags &
              vk::QueueFlagBits::eCompute))
        {
          chosen_compute_index = std::pair<int, int>(i, 0);
        }
      } // i

      if (queue_family_index_choice_cb != nullptr)
      {
        _auxiliary_queue_family_indexes = (*queue_family_index_choice_cb)(
          *_physical_device,
          queue_family_properties,
          chosen_compute_index,
          queue_family_index_choice_cb__user_pointer);
      }

      if (chosen_compute_index.first == static_cast<uint32_t>(-1))
      {
        throw std::runtime_error("No compute queue available on chosen device.");
      }

      _compute_queue_family_index = chosen_compute_index;

      const char *device_extensions_to_look_for[] = {
          "VK_KHR_portability_subset"
      };
      
      std::vector<std::string> device_extension_names_to_propose;
      std::vector<vk::ExtensionProperties> available_device_extensions =
        _physical_device->enumerateDeviceExtensionProperties();
      for (int i = 0; i < available_device_extensions.size(); i++)
      {
        for (int j = 0; j < (sizeof(device_extensions_to_look_for)/sizeof(device_extensions_to_look_for[0])); j++)
        {
          if (std::strcmp(available_device_extensions[i].extensionName,
            device_extensions_to_look_for[j]) == 0)
          {
            device_extension_names_to_propose.push_back(device_extensions_to_look_for[j]);
          }
        } // j
      } // i
      std::vector<std::string> device_extension_names_chosen;
      if (device_extension_choice_cb != nullptr)
      {
        device_extension_names_chosen =
          (*device_extension_choice_cb)(
            device_extension_names_to_propose,
            available_device_extensions,
            device_extension_choice_cb__user_pointer);
      } else
      {
        device_extension_names_chosen =
          device_extension_names_to_propose;
      }
      std::vector<const char *> device_extension_names_to_use;
      for (int l = 0; l < device_extension_names_chosen.size(); l++)
      {
        device_extension_names_to_use.push_back(
          device_extension_names_chosen.at(l).c_str()
        );
      }

#if !defined(BUILD_PYTHON_BINDINGS)
      if (device_extension_names_to_use.size() > 0)
      {
        fprintf(stderr, "Using device extensions:\n");
        for (int i = 0; i < device_extension_names_to_use.size(); i++)
        {
          fprintf(stderr, "  %s\n", device_extension_names_to_use[i]);
        }
      }
#endif // !defined(BUILD_PYTHON_BINDINGS)

      std::vector<uint32_t> unique_queue_family;
      std::vector<uint32_t> unique_queue_family_count;
      unique_queue_family.push_back(chosen_compute_index.first);
      unique_queue_family_count.push_back(1);
      for (int l = 0; l < _auxiliary_queue_family_indexes.size(); l++)
      {
        bool matched_existing = false;
        for (int k = 0; k < unique_queue_family.size(); k++)
        {
          if (unique_queue_family[k] == _auxiliary_queue_family_indexes[l].first)
          {
            unique_queue_family_count[k] = unique_queue_family_count[k] + 1;
            matched_existing = true;
          }
        }
        if (!matched_existing)
        {
          unique_queue_family.push_back(
            _auxiliary_queue_family_indexes[l].first);
          unique_queue_family_count.push_back(1);
        }
      }

      const float queue_priority = 1.0f;
      std::vector<vk::DeviceQueueCreateInfo> queues_to_use;
      for (int l = 0; l < unique_queue_family.size(); l++)
      {
        queues_to_use.emplace_back(
          vk::DeviceQueueCreateFlags(),
          static_cast<uint32_t>(unique_queue_family[l]),
          unique_queue_family_count[l],
          &queue_priority
        );
      }

      vk::DeviceCreateInfo device_create_info(
        vk::DeviceCreateFlags(),
        queues_to_use,
        {},
        device_extension_names_to_use
      );

      _device = std::make_unique<vk::raii::Device>(
        _physical_device->createDevice(device_create_info));
      
      vk::PipelineCacheCreateInfo pipeline_cache_info;

      _pipeline_cache = std::make_unique<vk::raii::PipelineCache>(
        *_device, pipeline_cache_info
      );

      for (int p = -1; p < int(_auxiliary_queue_family_indexes.size()); p++)
      {
        const std::pair<uint32_t, uint32_t> this_queue_family_index = 
          (p < 0) ?
            _compute_queue_family_index : 
            _auxiliary_queue_family_indexes[p];
        std::shared_ptr<vk::raii::Queue> opened_queue =
          std::make_shared<vk::raii::Queue>(
            *_device,
            this_queue_family_index.first,
            this_queue_family_index.second);
        if (p < 0)
        {
          _compute_queue = opened_queue;
          _compute_queue_mutex = std::make_unique<std::mutex>();
        } else
        {
          _auxiliary_queues.push_back(opened_queue);
          _auxiliary_queues_mutex.push_back(
            std::make_unique<std::mutex>());
        }

      } // p

      // Make memory pools.
      lmp_device =
        std::make_unique<LinearDeviceMemoryPool>(
          std::move(
            LinearDeviceMemoryPool(
              *_physical_device, *_device
            )
          )
        );
      lmp_staging =
        std::make_unique<LinearStagingMemoryPool>(
          std::move(
            LinearStagingMemoryPool(
              *_physical_device, *_device
            )
          )
        );

    }

  public:
    static std::shared_ptr<Context> create(
      std::vector<std::string> (*instance_extension_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::ExtensionProperties> &available,
        void *user_pointer),
      void *instance_extension_choice_cb__user_pointer,
      std::vector<std::string> (*instance_layer_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::LayerProperties> &available,
        void *user_pointer),
      void *instance_layer_choice_cb__user_pointer,
      uint32_t (*physical_device_choice_cb)(
        vk::raii::Instance const &instance,
        std::vector<vk::raii::PhysicalDevice> &physical_device_options,
        void *user_pointer),
      void *physical_device_choice_cb__user_pointer,
      std::vector<std::pair<uint32_t, uint32_t> > (*queue_family_index_choice_cb)(
        vk::raii::PhysicalDevice const &physical_device,
        std::vector<vk::QueueFamilyProperties> &queue_family_properties,
        std::pair<uint32_t, uint32_t> &chosen_compute_index,
        void *user_pointer),
      void *queue_family_index_choice_cb__user_pointer,
      std::vector<std::string> (*device_extension_choice_cb)(
        const std::vector<std::string> &suggested,
        const std::vector<vk::ExtensionProperties> &available,
        void *user_pointer),
      void *device_extension_choice_cb__user_pointer)
    {
      std::shared_ptr<Context> vk_ctxt =
        std::make_shared<Context>(std::move(Context(
          instance_extension_choice_cb,
          instance_extension_choice_cb__user_pointer,
          instance_layer_choice_cb,
          instance_layer_choice_cb__user_pointer,
          physical_device_choice_cb,
          physical_device_choice_cb__user_pointer,
          queue_family_index_choice_cb,
          queue_family_index_choice_cb__user_pointer,
          device_extension_choice_cb,
          device_extension_choice_cb__user_pointer
        )));
      return vk_ctxt;
    }

    template <typename T>
    std::shared_ptr<StorageTensor> storageTensor(
      size_t element_count)
    {
      std::shared_ptr<StorageTensor> storage =
        std::make_shared<StorageTensor>(
          std::move(
            StorageTensor(
              *_physical_device, *_device, *(lmp_device.get()), element_count * sizeof(T)
            )
          )
        );
      
      _tensor.push_back(storage);
      return storage;
    }

    template <typename T>
    std::shared_ptr<SharedTensor<T> > sharedTensor(
      size_t element_count)
    {
      std::shared_ptr<SharedTensor<T> > shared(
        std::make_shared<SharedTensor<T> >(
          std::move(
            SharedTensor<T>(
              *_physical_device, *_device, *(lmp_staging.get()), 
              *(lmp_device.get()), element_count
            )
          )
        )
      );
      
      _tensor.push_back(shared);
      return shared;
    }

    std::shared_ptr<TensorParameterSet> tensorParameterSet(
      const std::vector<std::shared_ptr<Tensor> > &tensor_set)
    {
      std::shared_ptr<TensorParameterSet> tensor_params(
        std::make_shared<TensorParameterSet>(
          std::move(
            TensorParameterSet(
              *_device, tensor_set
            )
          )
        )
      );

      _tensor_parameter_set.push_back(tensor_params);
      return tensor_params;
    }

    std::shared_ptr<Program> program(
      std::vector<ConstantBase> const &spec_constants,
      std::vector<ConstantBase> const &push_constants,
      std::shared_ptr<TensorParameterSet> const &params,
      std::vector<uint32_t> const &spirv,
      vk::raii::DescriptorSetLayout const *extra_descriptor_set = nullptr)
    {
      std::shared_ptr<Program> program(
        std::make_shared<Program>(
          std::move(
            Program(
              *_device, spec_constants, push_constants,
              *params, spirv, extra_descriptor_set
            )
          )
        )
      );

      _program.push_back(program);
      return program;
    }

    std::shared_ptr<Schema> schema()
    {
      std::shared_ptr<Schema> schema(
        std::make_shared<Schema>(
          std::move(
            Schema(
              *_physical_device, *_device,
              *_compute_queue, *(_compute_queue_mutex.get()),
              _compute_queue_family_index.first
            )
          )
        )
      );

      _schema.push_back(schema);
      return schema;
    }

    vk::Instance instance()
    {
      return **_instance;
    }

    vk::raii::PhysicalDevice const &physical_device()
    {
      return *_physical_device;
    }

    vk::raii::Device const &device() const
    {
      return *_device;
    }

    vk::raii::PipelineCache const &pipeline_cache() const
    {
      return *_pipeline_cache;
    }

    std::pair<uint32_t, uint32_t> computeQueueFamilyIndex() const
    {
      return _compute_queue_family_index;
    }

    uint32_t computeQueueFamily() const
    {
      return _compute_queue_family_index.first;
    }

    uint32_t computeQueueIndex() const
    {
      return _compute_queue_family_index.second;
    }

    std::shared_ptr<vk::raii::Queue> const &computeQueueFamilyQueue() const
    {
      return _compute_queue;
    }

    std::mutex *computeQueueMutex()
    {
      return _compute_queue_mutex.get();
    }

    size_t auxiliaryQueueCount() const
    {
      return _auxiliary_queue_family_indexes.size();
    }

    std::pair<uint32_t, uint32_t> auxiliaryQueueFamilyIndex(int aux_queue_idx) const
    {
      return _auxiliary_queue_family_indexes[aux_queue_idx];
    }

    uint32_t auxiliaryQueueFamily(int aux_queue_idx) const
    {
      return _auxiliary_queue_family_indexes[aux_queue_idx].first;
    }

    uint32_t auxiliaryQueueIndex(int aux_queue_idx) const
    {
      return _auxiliary_queue_family_indexes[aux_queue_idx].second;
    }

    std::shared_ptr<vk::raii::Queue> const &auxiliaryQueueFamilyQueue(int aux_queue_idx) const
    {
      return _auxiliary_queues[aux_queue_idx];
    }

    std::mutex *auxiliaryQueueMutex(int aux_queue_idx)
    {
      return _auxiliary_queues_mutex[aux_queue_idx].get();
    }

    void dryrunStorageTensorAllocate(uintmax_t size_bytes)
    {
      lmp_device->dryrunAllocate(size_bytes);
    }

    void dryrunSharedTensorAllocate(uintmax_t size_bytes)
    {
      lmp_device->dryrunAllocate(size_bytes);
      lmp_staging->dryrunAllocate(size_bytes);
    }

    void clear()
    {
      _tensor_parameter_set.clear();
      _program.clear();
      _schema.clear();
      _tensor.clear();
      lmp_device =
        std::make_unique<LinearDeviceMemoryPool>(
          std::move(
            LinearDeviceMemoryPool(
              *_physical_device, *_device
            )
          )
        );
      lmp_staging =
        std::make_unique<LinearStagingMemoryPool>(
          std::move(
            LinearStagingMemoryPool(
              *_physical_device, *_device
            )
          )
        );
    }

  private:
    std::unique_ptr<vk::raii::Context> _context;
    std::unique_ptr<vk::raii::Instance> _instance;
    std::unique_ptr<vk::raii::PhysicalDevice> _physical_device;
    std::unique_ptr<vk::raii::Device> _device;
    std::unique_ptr<vk::raii::PipelineCache> _pipeline_cache;
    std::unique_ptr<LinearDeviceMemoryPool> lmp_device;
    std::unique_ptr<LinearStagingMemoryPool> lmp_staging;
    uint32_t _physical_device_index;

    std::pair<uint32_t, uint32_t> _compute_queue_family_index;
    std::unique_ptr<std::mutex> _compute_queue_mutex;
    std::shared_ptr<vk::raii::Queue> _compute_queue;

    std::vector<std::pair<uint32_t, uint32_t> > _auxiliary_queue_family_indexes;
    std::vector<std::unique_ptr<std::mutex> > _auxiliary_queues_mutex;
    std::vector<std::shared_ptr<vk::raii::Queue> > _auxiliary_queues;

    std::shared_ptr<vk::raii::Queue> _graphics_queue;
    std::shared_ptr<vk::raii::Queue> _present_queue;

    std::vector<std::shared_ptr<Tensor> > _tensor;
    std::vector<std::shared_ptr<Schema> > _schema;
    std::vector<std::shared_ptr<Program> > _program;
    std::vector<std::shared_ptr<TensorParameterSet> > _tensor_parameter_set;
  };
}
