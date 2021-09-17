
This repo attempts to establish the relative benefit of using a 'VkQueue per thread' vs 'single VkQueue shared across all threads'.

"VkQueue per thread" means that N threads are submitting command buffers to N VkQueue's.

"single VkQueue shared across all threads" means that there are N threads recording work, then waiting on a mutex which guards a single VkQueue. When they acquire the mutex, they submit their work.

The test records and submits command buffers multiple times per thread. Currently, the command buffers just call `vkCmdSetViewport` and `vkCmdSetScissor` over and over.

The test will, on each thread, record a command buffer which calls ``vkCmdSetViewport` then `vkCmdSetScissor` 100 times, submits it and waits on the associated VkFence. The threads then do this 100 times before exiting.


On a GTX 1060, with 6 CPU threads: (driver version 470)

Run with a VkQueue per thread
Time taken: 36,595,043ns

Run with a single VkQueue shared with all thread guarded by a mutex
Time taken: 4,578,753ns

Using a single VkQueue is 7.99236 times faster than using multiple VkQueues

TODO:
Do actual work instead of just changing the viewport state. These results may just be hitting a driver fast path, so its not without reason to use a massive grain of salt.

Note: This test pretty much requires that you have an Nvidia card, since not much hardware has as many queues per queue family where assinging a VkQueue per thread makes sense.

