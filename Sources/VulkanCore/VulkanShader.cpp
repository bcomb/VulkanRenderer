#include <VulkanShader.h>
#include <VulkanHelper.h>

#include <spirv-headers/spirv.h>

/*****************************************************************************/
VkShaderStageFlagBits getShaderStage(SpvExecutionModel model)
{
	switch (model)
	{
	case SpvExecutionModelGLCompute:	return VK_SHADER_STAGE_COMPUTE_BIT;
	case SpvExecutionModelVertex:	return VK_SHADER_STAGE_VERTEX_BIT;
	case SpvExecutionModelFragment:	return VK_SHADER_STAGE_FRAGMENT_BIT;
	default:
		assert(!"unsupported model");
	};

	return VkShaderStageFlagBits(0);
}

/*****************************************************************************/
// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
void parseSpirv(VulkanShader& pShader, const uint32_t* code, uint32_t codeSize)
{
	assert(code[0] == SpvMagicNumber); // p19

	union SpvVersion
	{
		uint32_t raw;
		struct { uint8_t pad0, minor, major, pad1; };
	} version;
	version.raw = code[1];

	uint32_t idBound = code[3];

	// Instruction stream
	const uint32_t* stream = code + 5;
	while (stream != code + codeSize)
	{
		union SpvOpCode
		{
			uint32_t raw;
			struct { uint16_t opcode, wordCount; };
		} opcode;
		opcode.raw = *stream;

		switch (opcode.opcode)
		{
		case SpvOpEntryPoint:
		{
			assert(opcode.wordCount >= 2);
			pShader.mStage = getShaderStage(SpvExecutionModel(stream[1]));
			return; // stop parsing
		}
		break;
		}

		stream += opcode.wordCount;
	}
}

/*****************************************************************************/
VulkanShader VulkanShader::loadFromFile(VkDevice pDevice, const std::string& pFilename)
{
	VulkanShader lShader = {};
	FILE* file = fopen(pFilename.c_str(), "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		long bytesSize = ftell(file);
		assert(bytesSize);
		fseek(file, 0, SEEK_SET);

		char* code = (char*)malloc(bytesSize);
		assert(code);
		size_t bytesRead = fread(code, 1, bytesSize, file);
		assert(bytesRead == bytesSize);
		assert(bytesRead % 4 == 0);

		// Extract information directly from spirv
		parseSpirv(lShader, (const uint32_t*)code, bytesSize / 4);

		VkShaderModuleCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		//VkShaderModuleCreateFlags    flags;
		lCreateInfo.codeSize = bytesSize;			// size in bytes
		lCreateInfo.pCode = (const uint32_t*)code;	// must be an array of codeSize/4

		VkShaderModule lShaderModule = {};
		VK_CHECK(vkCreateShaderModule(pDevice, &lCreateInfo, nullptr, &lShaderModule));
		lShader.mShaderModule = lShaderModule;
		free(code);
	}

	return lShader;
}