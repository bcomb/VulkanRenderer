#include <volk.h>

#define VK_CHECK(call_) do { VkResult result_ = call_;	assert(result_ == VK_SUCCESS); } while(0);
#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof(array_[0]))

// Allocation on stack
// Automatically destroyed when exit function
#define STACK_ALLOC(TYPE_,COUNT_) (TYPE_*)_alloca(sizeof(TYPE_) * COUNT_)