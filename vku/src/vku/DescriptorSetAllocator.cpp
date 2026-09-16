#include "volk.h"
#include "vku/DescriptorSetAllocator.h"
#include "vku/Macros.h"
#include "vku/Log.h"

void vku::DescriptorSetAllocator::Init(IAllocator& alloc, VkDevice logical_device, uint32_t initialSetAmount, Vector<PoolSizeRatio> ratios)
{
	m_Ratios = ratios;

	VkDescriptorPool pool = CreatePool(alloc, logical_device, initialSetAmount);

	p_SetsPerPool = static_cast<uint32_t>(initialSetAmount * 1.5);
	m_FreePool.push_back(pool);
}

void vku::DescriptorSetAllocator::Reset(VkDevice device)
{
	for (auto& free : m_FreePool)
	{
		vkResetDescriptorPool(device, free, 0);
	}
	for (auto& full : m_FullPool)
	{
		vkResetDescriptorPool(device, full, 0);
		m_FreePool.push_back(full);
	}
	m_FullPool.clear();
}

void vku::DescriptorSetAllocator::Free(VkDevice device)
{
	for (auto& free : m_FreePool)
	{
		vkDestroyDescriptorPool(device, free, 0);
	}
	for (auto& full : m_FullPool)
	{
		vkDestroyDescriptorPool(device, full, 0);
	}
	m_FreePool.clear();
	m_FullPool.clear();
}

VkDescriptorSet vku::DescriptorSetAllocator::Allocate(IAllocator& alloc, VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
	VkDescriptorPool poolToUse = GetPool(alloc, device);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet set;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &set);

	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

		m_FullPool.push_back(poolToUse);

		poolToUse = GetPool(alloc, device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set));
	}

	m_FreePool.push_back(poolToUse);
	return set;
}

VkDescriptorPool vku::DescriptorSetAllocator::GetPool(IAllocator& alloc, VkDevice device)
{
	VkDescriptorPool newPool;
	if (m_FreePool.size() != 0) {
		newPool = m_FreePool.back();
		m_FreePool.pop_back();
	}
	else {
		//need to create a new pool
		newPool = CreatePool(alloc, device, p_SetsPerPool);

		p_SetsPerPool = p_SetsPerPool * 2;
		if (p_SetsPerPool > 4092) {
			p_SetsPerPool = 4092;
		}
	}

	return newPool;
}

VkDescriptorPool vku::DescriptorSetAllocator::CreatePool(IAllocator& alloc, VkDevice device, uint32_t setCount)
{
	STLAllocator<VkDescriptorPoolSize> dpsa(alloc);
	Vector<VkDescriptorPoolSize> poolSizes(dpsa);
	for (PoolSizeRatio ratio : m_Ratios) {
		poolSizes.push_back(VkDescriptorPoolSize{ ratio.m_DescriptorType, uint32_t(ratio.m_Ratio * p_SetsPerPool) });
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = setCount;
	pool_info.poolSizeCount = (uint32_t)poolSizes.size();
	pool_info.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool{};
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool));
	return newPool;
}
