cmake_minimum_required(VERSION 3.11)
project(VulkanMultiThreadedQueueTest)

find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        0408041547ad96ae05c6aae5c4af60f9b51bd6d9
)
FetchContent_MakeAvailable(vk_bootstrap)

add_executable(VulkanMultiThreadedQueueTest main.cpp)
target_link_libraries(VulkanMultiThreadedQueueTest PUBLIC vk-bootstrap Vulkan::Vulkan)