

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <assert.h>
#include <chrono>

const uint32_t command_buffer_size = 100;
const uint32_t repeat_submit = 100;

void record_work(VkCommandBuffer cmd_buf)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd_buf, &begin_info);
    for (uint32_t i = 0; i < command_buffer_size; i++)
    {
        VkViewport viewport{};
        viewport.width = 100;
        viewport.height = 100;
        vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
        VkRect2D scissor{};
        vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
    }
    vkEndCommandBuffer(cmd_buf);
}

struct Command
{
    VkCommandPool command_pool;
    VkCommandBuffer cmd_buf;
    VkFence fence;
};

Command create_command(VkDevice device)
{
    Command command{};
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkResult res = vkCreateFence(device, &fence_info, nullptr, &command.fence);
    assert(res == VK_SUCCESS);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = 0;
    res = vkCreateCommandPool(device, &pool_info, nullptr, &command.command_pool);
    assert(res == VK_SUCCESS);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandBufferCount = 1;
    alloc_info.commandPool = command.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    res = vkAllocateCommandBuffers(device, &alloc_info, &command.cmd_buf);
    assert(res == VK_SUCCESS);

    return command;
}
void destroy_command(VkDevice device, Command command)
{
    vkDestroyCommandPool(device, command.command_pool, nullptr);
    vkDestroyFence(device, command.fence, nullptr);
}

void work(VkDevice device, Command command, VkQueue queue, bool run_with_multiple_queues, std::mutex *mutex)
{
    for (int i = 0; i < repeat_submit; i++)
    {
        record_work(command.cmd_buf);
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command.cmd_buf;
        if (run_with_multiple_queues)
        {
            vkQueueSubmit(queue, 1, &submit_info, command.fence);
        }
        else
        {
            std::lock_guard<std::mutex> lg(*mutex);
            vkQueueSubmit(queue, 1, &submit_info, command.fence);
        }
        vkWaitForFences(device, 1, &command.fence, true, 10000000000);
        vkResetFences(device, 1, &command.fence);
        vkResetCommandBuffer(command.cmd_buf, 0);
    }
}

auto run(bool run_with_multiple_queues)
{
    vkb::InstanceBuilder instance_builder;
    auto instance_ret =
        instance_builder.request_validation_layers(false)
            .require_api_version(1, 2)
            .use_default_debug_messenger()
            .set_headless()
            .build();
    if (!instance_ret)
    {
        std::cerr << instance_ret.error().message() << "\n";
        assert(false);
    }
    vkb::Instance instance = instance_ret.value();

    vkb::PhysicalDeviceSelector phys_device_selector(instance);
    auto phys_device_ret = phys_device_selector
                               .select();
    if (!phys_device_ret)
    {
        std::cerr << phys_device_ret.error().message() << "\n";
        assert(false);
    }

    auto queue_families = phys_device_ret.value().get_queue_families();
    vkb::DeviceBuilder device_builder{phys_device_ret.value()};
    if (run_with_multiple_queues)
    {

        device_builder.custom_queue_setup({vkb::CustomQueueDescription{
            0,
            queue_families[0].queueCount, std::vector<float>(queue_families[0].queueCount, 0.5f)}});
    }
    // else - use default 1 queue

    auto device_ret = device_builder.build();
    if (!device_ret)
    {
        std::cerr << device_ret.error().message() << "\n";
        assert(false);
    }
    vkb::Device device = device_ret.value();
    VkDevice dev = device.device;

    std::mutex common_mutex;
    VkQueue common_queue = device.get_queue(vkb::QueueType::graphics).value();

    std::vector<Command> commands;
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < 6; i++)
    {
        Command command = create_command(dev);
        VkQueue queue = common_queue;
        if (run_with_multiple_queues)
        {
            vkGetDeviceQueue(dev, 0, i, &queue);
        }
        threads.emplace_back(work, dev, command, queue, run_with_multiple_queues, &common_mutex);
        commands.push_back(command);
    }
    auto start_time = std::chrono::system_clock::now();
    for (auto &t : threads)
    {
        t.join();
    }
    auto end_time = std::chrono::system_clock::now();
    auto time_taken = (end_time - start_time).count();
    std::cout << "Time taken: " << time_taken << "ns\n";

    for (uint32_t i = 0; i < queue_families.size(); i++)
    {
        destroy_command(dev, commands[i]);
    }

    vkb::destroy_device(device);
    vkb::destroy_instance(instance);
    return time_taken;
}
int main()
{
    std::cout << "Run with a dedicated VkQueue per thread\n";
    auto first = run(true);
    std::cout << "Run with a single VkQueue shared with all thread guarded by a mutex\n";
    auto second = run(false);
	std::cout << "First run / Second run " << double(first)/second << "\n";
}
