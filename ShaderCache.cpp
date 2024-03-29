
#include "ShaderCache.h"
#include "Escort.h"
#include "Settings.h"

#include <unordered_set>
#include <D3Dcompiler.h>
#include <fstream>
#include <boost/signals2/mutex.hpp>

#include "KenshiLib/Include/core/md5.h"

HRESULT(*D3DCompile_orig)(LPCVOID pSrcData,
	SIZE_T                 SrcDataSize,
	LPCSTR                 pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude* pInclude,
	LPCSTR                 pEntrypoint,
	LPCSTR                 pTarget,
	UINT                   Flags1,
	UINT                   Flags2,
	ID3DBlob** ppCode,
	ID3DBlob** ppErrorMsgs
	);

struct ShaderInclude
{
	ShaderInclude()
		: type((D3D_INCLUDE_TYPE)-1), name(""), hash(0)
	{

	}
	ShaderInclude(D3D_INCLUDE_TYPE type, std::string name, size_t hash)
	{
		this->type = type;
		this->name = name;
		this->hash = hash;
	}

	void Serialize(std::vector<char>& bytes) const;

	static ShaderInclude Deserialize(std::vector<char>& bytes, size_t& offset);

	std::string name;
	D3D_INCLUDE_TYPE type;
	size_t hash;
};

class Shader
{
public:
	Shader() { compiled = nullptr; };
	Shader(LPCVOID pSrcData, SIZE_T SrcDataSize, const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude,
		LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2)
	{
		source = std::string((const char*)pSrcData, SrcDataSize);
		// Bugfix - LPCSTR can be null, but std::string can't be initialized from nullptr
		entrypoint = "";
		if(pEntrypoint != nullptr)
			entrypoint = pEntrypoint;
		target = "";
		// this should never happen - error is printed elsewhere
		if (pTarget != nullptr)
			target = pTarget;
		flags1 = Flags1;
		flags2 = Flags2;
		while (pDefines != NULL && pDefines->Name != NULL && pDefines->Definition != NULL)
		{
			defines.push_back(std::make_pair(pDefines->Name, pDefines->Definition));
			++pDefines;
		}
		compiled = nullptr;
		compiledSize = 0;
	}

	bool operator==(const Shader& other) const
	{
		return (source == other.source
			&& std::equal(defines.begin(), defines.end(), other.defines.begin())
			// includes aren't used as hash keys as you don't know their contents until
			// after you've got the Shader from the hashmap
			//&& include == other.include
			&& entrypoint == other.entrypoint
			&& target == other.target
			&& flags1 == other.flags1
			&& flags2 == other.flags2);
	}
	
	void SetBlob(ID3DBlob* blob)
	{
		compiledSize = blob->GetBufferSize();
		compiled = new char[compiledSize];
		memcpy(compiled, blob->GetBufferPointer(), compiledSize);
	}

	void AddInclude(ShaderInclude include)
	{
		includes.push_back(include);
	}

	// convert to byte array
	void Serialize(std::vector<char> &bytes) const;
	static Shader Deserialize(std::vector<char>& bytes, size_t &offset);

	std::string source;
	std::vector<std::pair<std::string, std::string>> defines;
	std::vector<ShaderInclude> includes;
	std::string entrypoint;
	std::string target;
	UINT flags1;
	UINT flags2;
	char* compiled;
	size_t compiledSize;
};

// Stolen from Boost
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
	// shitty vector hash
	template <typename T>
	struct hash<std::vector<T>>
	{
		std::size_t operator()(const std::vector<T> vec) const
		{
			size_t hash = 0x12345678;
			std::hash<T> hasher = std::hash<T>();
			for (int i = 0; i < vec.size(); ++i)
			{
				// this is bad and I don't care
				hash ^= hasher(vec[i]);
			}

			return hash;
		}
	};

	// shitty touple hash
	template <typename T1, typename T2>
	struct hash<std::pair<T1, T2>>
	{
		std::size_t operator()(const std::pair<T1, T2> pair) const
		{
			size_t hash = std::hash<T1>()(pair.first);
			hash_combine(hash, pair.second);

			return hash;
		}
	};

	template <>
	struct hash<Shader>
	{
		std::size_t operator()(const Shader& k) const
		{
			size_t hash = std::hash<std::string>()(k.source);
			hash_combine(hash, k.defines);
			// includes aren't used as hash keys as you don't know their contents until
			// after you've got the Shader from the hashmap
			//hash_combine(hash, k.includes);
			hash_combine(hash, k.entrypoint);
			hash_combine(hash, k.target);
			hash_combine(hash, k.flags1);
			hash_combine(hash, k.flags2);
			return hash;
		}
	};

}

class ShaderCacheFile
{
public:
	ShaderCacheFile(std::string filename);

	// fetches from cache or compiles + writes to disk
	ID3DBlob* GetBlob(LPCVOID pSrcData,
		SIZE_T                 SrcDataSize,
		LPCSTR                 pSourceName,
		const D3D_SHADER_MACRO* pDefines,
		ID3DInclude* pInclude,
		LPCSTR                 pEntrypoint,
		LPCSTR                 pTarget,
		UINT                   Flags1,
		UINT                   Flags2,
		ID3DBlob** ppCode,
		ID3DBlob** ppErrorMsgs,
		HRESULT* result);

private:

	void Serialize();

	std::string filename;
	std::unordered_set<Shader> cachedShaders;
	// RAM cache of full file so we don't have to re-read every time we have to recalculate hash
	std::vector<char> fileBytes;
};

// Shader Cache File Header v02
#define SCFHMAGIC "SCFH02"

struct ShaderCacheFileHeader
{
	ShaderCacheFileHeader()
	{
		memcpy(magic, SCFHMAGIC, strlen(SCFHMAGIC));
	}
	char magic[6];
	// hash of body
	char hash[33];
	size_t numShaders;
};

ShaderCacheFile::ShaderCacheFile(std::string filename)
{
	this->filename = filename;

	std::ifstream file(filename, std::ifstream::binary | std::ifstream::ate);
	// if file doesn't exist, no init needed
	if (!file.is_open())
	{
		DebugLog("Shader cache file doesn't exist");
		return;
	}

	DebugLog("Shader cache load start");

	// read file into memory
	fileBytes.resize(file.tellg());
	file.seekg(std::ifstream::beg);
	file.read(&fileBytes[0], fileBytes.size());

	size_t readpos = 0;
	// read header + check integridy
	ShaderCacheFileHeader header;
	if (fileBytes.size() < sizeof(header))
	{
		ErrorLog("Shader cache doesn't have full header, queueing cache clear");
		fileBytes.clear();
		return;
	}
	memcpy(&header, &fileBytes[readpos], sizeof(header));
	readpos += sizeof(header);
	if (strncmp(header.magic, SCFHMAGIC, 6) != 0)
	{
		ErrorLog("Shader cache magic number invalid, queueing cache clear");
		fileBytes.clear();
		return;
	}
	md5::md5_t hasher(&fileBytes[readpos], fileBytes.size() - readpos);
	char hashStr[33];
	hasher.get_string(hashStr);
	if (strncmp(header.hash, hashStr, 33) != 0)
	{
		ErrorLog("Shader cache hash is invalid, queueing cache clear");
		fileBytes.clear();
		return;
	}
	
	// If we get to this point - there's a valid header + the last write succeeded completely, 
	// so we should expect no errors/corruption

	for (size_t i = 0; i < header.numShaders; ++i)
	{
		Shader shader = Shader::Deserialize(fileBytes, readpos);
		if (cachedShaders.find(shader) != cachedShaders.end())
			ErrorLog("Duplicate shader");
		cachedShaders.insert(shader);
	}

	size_t remainingBytes = fileBytes.size() - readpos;
	if (remainingBytes != 0)
	{
		ErrorLog("Shader cache has extra bytes, queueing cache clear");
		cachedShaders.clear();
		fileBytes.clear();
		return;
	}

	DebugLog("Shader cache loaded successfully, " + std::to_string(cachedShaders.size()) + " cached shaders");
	return;
}

class IncludeHook : public ID3DInclude
{
public:
	IncludeHook(Shader* shader, ID3DInclude* hooked)
		: hadError(false)
	{
		this->shader = shader;
		this->hooked = hooked;
	}
	HRESULT Open(D3D_INCLUDE_TYPE includeType,
		LPCSTR           pFileName,
		LPCVOID          pParentData,
		LPCVOID* ppData,
		UINT* pBytes)
	{
		// Ogre uses a hard-coded include handler that doesn't use pParentData, so this is safe to ignore
		/*
		if (pParentData != nullptr)
		{
			hadError = true;
			ErrorLog("ID3DInclude::Open pParentData isn't null");
			ErrorLog(std::string((char*)pParentData));
		}
		*/

		HRESULT result = hooked->Open(includeType, pFileName, pParentData, ppData, pBytes);

		if (result == S_OK)
		{
			std::string includeStr((char*)*ppData, *pBytes);
			shader->AddInclude(ShaderInclude(includeType, pFileName, std::hash<std::string>()(includeStr)));
		}
		else
		{
			hadError = true;
			ErrorLog("ID3DInclude::Open error getting include: " + std::string(pFileName));
		}

		return result;
	}

	HRESULT Close(LPCVOID pData)
	{
		return hooked->Close(pData);
	}

	bool HadError()
	{
		return hadError;
	}

private:
	ID3DInclude* hooked;
	Shader* shader;
	bool hadError;
};

// WARNING - YOU MUST HOLD A SHADER LOCK TO CALL THIS
ID3DBlob* ShaderCacheFile::GetBlob(LPCVOID pSrcData,
	SIZE_T                 SrcDataSize,
	LPCSTR                 pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude* pInclude,
	LPCSTR                 pEntrypoint,
	LPCSTR                 pTarget,
	UINT                   Flags1,
	UINT                   Flags2,
	ID3DBlob** ppCode,
	ID3DBlob** ppErrorMsgs,
	HRESULT *result)
{
	// The bug in 0.2.5 was due to improperly handled optional inputs
	// pSourceName - not used internally, so not really an issue (docs say used for error messages)
	// pDefines - already has nullptr check
	// pInclude - NOT IMPLEMENTED - this is implemented by the user...
	// pEntryPoint - this needs nullptr check in Shader() (done)
	// ppErrorMsgs - nullptr check (done)

	std::string shaderName = "[no name]";
	if (pSourceName != nullptr)
		shaderName = pSourceName;
	
	// sanity check
	if (pTarget == nullptr)
	{
		ErrorLog("Shader missing target!");
		ErrorLog(shaderName);
	}

	Shader shader = Shader(pSrcData, SrcDataSize, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2);
	std::unordered_set<Shader>::iterator cachedShader = cachedShaders.find(shader);
	if (cachedShader != cachedShaders.end())
	{
		// check includes (write to new shader, but use include names/hashes from cache)
		bool includesMatch = true;
		for (int i = 0; i < cachedShader->includes.size(); ++i)
		{
			uint32_t includeSize;
			char* includeBytes;
			// Ogre uses a hard-coded include handler that doesn't use pParentData, so passing null here should be fine
			if (pInclude->Open(cachedShader->includes[i].type, cachedShader->includes[i].name.c_str(), nullptr, (LPCVOID*)&includeBytes, &includeSize) == S_OK)
			{
				std::string includeStr(includeBytes, includeSize);
				size_t newHash = std::hash<std::string>()(includeStr);
				if (newHash != cachedShader->includes[i].hash)
				{
					DebugLog("Shader: " + shaderName + " include changed: " + cachedShader->includes[i].name);
					includesMatch = false;
					break;
				}
			}
			else
			{
				ErrorLog("Error opening include: " + cachedShader->includes[i].name);
				includesMatch = false;
				break;
			}
		}
		if (includesMatch)
		{
			//DebugLog("Shader is cached");
			D3DCreateBlob(cachedShader->compiledSize, ppCode);
			memcpy((*ppCode)->GetBufferPointer(), cachedShader->compiled, cachedShader->compiledSize);
			*result = S_OK;
			// Bugfix for 0.2.5 - this is sometimes null? Possibly from ReShade?
			if (ppErrorMsgs)
				*ppErrorMsgs = nullptr;
			return *ppCode;
		}
		// else, recompile
		// TODO delete old shader?
		// HACK this will trigger a full reserialize
		fileBytes.clear();
	}

	//DebugLog("Shader is uncached");

	IncludeHook includeHook(&shader, pInclude);

	// if the shader isn't cached, compile it and re-serialize
	*result = D3DCompile_orig(pSrcData, SrcDataSize, pSourceName, pDefines, &includeHook, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);

	// this SOMETIMES happens?!?
	if (ppCode == nullptr || *ppCode == nullptr || *result != S_OK)
	{
		std::stringstream err;
		err << "D3DCompile error: " << *result;
		ErrorLog(err.str());
		ErrorLog(shaderName);
		return nullptr;
	}

	if (includeHook.HadError())
	{
		DebugLog("Include error - skipping cache for " + shaderName);
		return *ppCode;
	}

	shader.SetBlob(*ppCode);

	// above code will already trigger a cache clear if this is a duplicate
	cachedShaders.insert(shader);

	if (fileBytes.size() > 0)
	{
		// concat onto file and update header
		// need to use std::ofstream::in for dumb reasons
		// https://stackoverflow.com/questions/28999745/stdofstream-with-stdate-not-opening-at-end
		std::ofstream file(filename, std::ofstream::in | std::ofstream::binary | std::ofstream::ate);
		size_t offset = fileBytes.size();
		// write shader
		shader.Serialize(fileBytes);
		file.write(&fileBytes[offset], fileBytes.size() - offset);
		// update header
		ShaderCacheFileHeader header;
		header.numShaders = cachedShaders.size();
		md5::md5_t hasher(&fileBytes[sizeof(ShaderCacheFileHeader)], fileBytes.size() - sizeof(ShaderCacheFileHeader));
		hasher.get_string(header.hash);
		file.seekp(std::ofstream::beg);
		file.write((char*)&header, sizeof(header));
		file.close();
		//DebugLog("Appended shader to cache");
	}
	else
	{
		// need to generate whole file
		DebugLog("Regenerating shader cache file");
		Serialize();
	}
	return *ppCode;
}

void ShaderCacheFile::Serialize()
{
	std::ofstream file(filename, std::ofstream::binary);
	ShaderCacheFileHeader header;
	header.numShaders = cachedShaders.size();
	fileBytes.resize(sizeof(header));
	size_t shadersStart = fileBytes.size();

	// Serialize cached shaders
	for (std::unordered_set<Shader>::iterator shader = cachedShaders.begin(); shader != cachedShaders.end(); ++shader)
	{
		shader->Serialize(fileBytes);
	}

	md5::md5_t hasher(&fileBytes[shadersStart], fileBytes.size() - shadersStart);
	hasher.get_string(header.hash);

	// write header
	file.write((char*)&header, sizeof(header));
	// write shaders
	file.write(&fileBytes[shadersStart], fileBytes.size() - shadersStart);
	file.close();
}

static void WriteString(std::string str, std::vector<char>& bytes)
{
	size_t writePos = bytes.size();
	int32_t sourceLen = str.size();
	bytes.resize(bytes.size() + sizeof(sourceLen));
	memcpy(&bytes[writePos], &sourceLen, sizeof(sourceLen));
	writePos = bytes.size();
	bytes.resize(bytes.size() + sourceLen);
	memcpy(&bytes[writePos], str.c_str(), sourceLen);
}

void ShaderInclude::Serialize(std::vector<char>& bytes) const
{
	WriteString(name, bytes);

	size_t writePos = bytes.size();
	bytes.resize(bytes.size() + sizeof(type));
	memcpy(&bytes[writePos], &type, sizeof(type));

	writePos = bytes.size();
	bytes.resize(bytes.size() + sizeof(hash));
	memcpy(&bytes[writePos], &hash, sizeof(hash));
}

void Shader::Serialize(std::vector<char>& bytes) const
{
	WriteString(source, bytes);
	// write defines
	size_t writePos = bytes.size();
	int32_t numDefines = defines.size();
	bytes.resize(bytes.size() + sizeof(numDefines));
	memcpy(&bytes[writePos], &numDefines, sizeof(numDefines));
	for (size_t i = 0; i < numDefines; ++i)
	{
		WriteString(defines[i].first, bytes);
		WriteString(defines[i].second, bytes);
	}

	// includes
	writePos = bytes.size();
	int32_t numIncludes = includes.size();
	bytes.resize(bytes.size() + sizeof(numIncludes));
	memcpy(&bytes[writePos], &numIncludes, sizeof(numIncludes));
	for (size_t i = 0; i < numIncludes; ++i)
	{
		includes[i].Serialize(bytes);
	}

	// entrypoint
	WriteString(entrypoint, bytes);
	// target
	WriteString(target, bytes);
	// flags
	writePos = bytes.size();
	bytes.resize(bytes.size() + sizeof(flags1) + sizeof(flags2));
	memcpy(&bytes[writePos], &flags1, sizeof(flags1));
	writePos += sizeof(flags1);
	memcpy(&bytes[writePos], &flags2, sizeof(flags2));
	// compiled blob
	writePos = bytes.size();
	bytes.resize(bytes.size() + sizeof(compiledSize));
	memcpy(&bytes[writePos], &compiledSize, sizeof(compiledSize));
	writePos = bytes.size();
	bytes.resize(bytes.size() + compiledSize);
	memcpy(&bytes[writePos], compiled, compiledSize);
}

static std::string ReadString(std::vector<char>& bytes, size_t& offset)
{
	int32_t sourceLen;
	memcpy(&sourceLen, &bytes[offset], sizeof(sourceLen));
	offset += sizeof(sourceLen);
	// bootleg way of constructing a string in memory
	char* str = new char[sourceLen + 1];
	str[sourceLen] = '\00';
	memcpy(str, &bytes[offset], sourceLen);
	offset += sourceLen;
	return std::string(str);
}

ShaderInclude ShaderInclude::Deserialize(std::vector<char>& bytes, size_t& offset)
{
	std::string name = ReadString(bytes, offset);

	D3D_INCLUDE_TYPE type;
	memcpy(&type, &bytes[offset], sizeof(type));
	offset += sizeof(type);

	size_t hash;
	memcpy(&hash, &bytes[offset], sizeof(hash));
	offset += sizeof(hash);

	return ShaderInclude(type, name, hash);
}

Shader Shader::Deserialize(std::vector<char>& bytes, size_t& offset)
{
	Shader shader;
	shader.source = ReadString(bytes, offset);
	// read defines
	int32_t numDefines;
	memcpy(&numDefines, &bytes[offset], sizeof(numDefines));
	offset += sizeof(numDefines);
	shader.defines.resize(numDefines);
	for (size_t i = 0; i < numDefines; ++i)
	{
		shader.defines[i].first = ReadString(bytes, offset);
		shader.defines[i].second = ReadString(bytes, offset);
	}

	// read includes
	int32_t numIncludes;
	memcpy(&numIncludes, &bytes[offset], sizeof(numIncludes));
	offset += sizeof(numIncludes);
	shader.includes.resize(numIncludes);
	for (size_t i = 0; i < numIncludes; ++i)
	{
		shader.includes[i] = ShaderInclude::Deserialize(bytes, offset);
	}

	// entrypoint
	shader.entrypoint = ReadString(bytes, offset);
	// target
	shader.target = ReadString(bytes, offset);

	// flags
	memcpy(&shader.flags1 , &bytes[offset], sizeof(shader.flags1));
	offset += sizeof(shader.flags1);
	memcpy(&shader.flags2, &bytes[offset], sizeof(shader.flags2));
	offset += sizeof(shader.flags2);

	// compiled blob
	memcpy(&shader.compiledSize, &bytes[offset], sizeof(shader.compiledSize));
	offset += sizeof(shader.compiledSize);
	shader.compiled = new char[shader.compiledSize];
	memcpy(shader.compiled, &bytes[offset], shader.compiledSize);
	offset += shader.compiledSize;

	return shader;
}

bool printed = false;
ShaderCacheFile* shaderCache;
boost::signals2::mutex mutex;

HRESULT D3DCompile_hook(LPCVOID pSrcData,
	SIZE_T                 SrcDataSize,
	LPCSTR                 pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude*           pInclude,
	LPCSTR                 pEntrypoint,
	LPCSTR                 pTarget,
	UINT                   Flags1,
	UINT                   Flags2,
	ID3DBlob** ppCode,
	ID3DBlob** ppErrorMsgs
)
{
	if (!printed)
	{
		DebugLog("First shader compile request");
		printed = true;
	}

	if(!Settings::GetCacheShaders())
		return D3DCompile_orig(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);

	std::stringstream msg;
	mutex.lock();
	if (!shaderCache)
	{
		ErrorLog("CRITICAL ERROR: ShaderCache not initialized!");
		shaderCache = new ShaderCacheFile("./RE_Kenshi/shader_cache.sc");
	}
	HRESULT ret;
	*ppCode = shaderCache->GetBlob(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs, &ret);
	mutex.unlock();

	return ret;
}



void ShaderCache::Init()
{
	// create folder if needed
	_wmkdir(L"./RE_Kenshi");

	mutex.lock();
	if (!shaderCache)
		shaderCache = new ShaderCacheFile("./RE_Kenshi/shader_cache.sc");
	mutex.unlock();

	// enable hook
	void* shaderCompileAddr = Escort::GetFuncAddress("D3DCompiler_43.dll", "D3DCompile");
	D3DCompile_orig = Escort::JmpReplaceHook<HRESULT(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude* pInclude
		, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob** )>
		(shaderCompileAddr, D3DCompile_hook, 6);
}