#include <VulkanDescriptor.h>
#include <VulkanHelper.h>

/******************************************************************************/
void DescriptorLayoutBuilder::addBinding(uint32_t pBinding, VkDescriptorType pType)
{
    VkDescriptorSetLayoutBinding lBinding = {};
    lBinding.binding = pBinding;
    lBinding.descriptorType = pType;
    lBinding.descriptorCount = 1;
    //lBinding.stageFlags = pStageFlags;   // fill during build
    mBindings.push_back(lBinding);
}

/******************************************************************************/
VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice pDevice, VkShaderStageFlags pStageFlags)
{
    for (auto& b : mBindings)
    {
        b.stageFlags = pStageFlags;
    }

    VkDescriptorSetLayoutCreateInfo lLayoutInfo = {};
    lLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //lLayoutInfo.flags
    lLayoutInfo.bindingCount = (uint32_t)mBindings.size();
    lLayoutInfo.pBindings = mBindings.data();

    VkDescriptorSetLayout lLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(pDevice, &lLayoutInfo, nullptr, &lLayout));
    return lLayout;
}

/******************************************************************************/
void DescriptorLayoutBuilder::clear()
{
    mBindings.clear();
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

void DescriptorAllocator::initPool(VkDevice pDevice, uint32_t pMaxSets, const std::vector<PoolSize>& pPoolSizes)
{
    std::vector<VkDescriptorPoolSize> lPoolSizes;
    for (auto& p : pPoolSizes)
    {
        lPoolSizes.push_back({ p.mType, p.mCount * pMaxSets });
    }

    VkDescriptorPoolCreateInfo lPoolInfo = {};
    lPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    lPoolInfo.poolSizeCount = (uint32_t)lPoolSizes.size();
    lPoolInfo.pPoolSizes = lPoolSizes.data();
    lPoolInfo.maxSets = pMaxSets;

    VK_CHECK(vkCreateDescriptorPool(pDevice, &lPoolInfo, nullptr, &mPool));
}

/******************************************************************************/
void DescriptorAllocator::clearDescriptors(VkDevice pDevice)
{
    VK_CHECK(vkResetDescriptorPool(pDevice, mPool, 0));
}

/******************************************************************************/
void DescriptorAllocator::destroyPool(VkDevice pDevice)
{
    vkDestroyDescriptorPool(pDevice, mPool, nullptr);
}

/******************************************************************************/
VkDescriptorSet DescriptorAllocator::allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout)
{
    VkDescriptorSetAllocateInfo lAllocInfo = {};
    lAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    lAllocInfo.descriptorPool = mPool;
    lAllocInfo.descriptorSetCount = 1;
    lAllocInfo.pSetLayouts = &pLayout;

    VkDescriptorSet lSet;
    VK_CHECK(vkAllocateDescriptorSets(pDevice, &lAllocInfo, &lSet));
    return lSet;
}