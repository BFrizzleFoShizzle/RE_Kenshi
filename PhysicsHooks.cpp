#include "PhysicsHooks.h"
#include "Escort.h"
#include "Settings.h"
#include <kenshi/PhysicsActual.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Kenshi.h>

#include <boost/unordered_map.hpp>

// TODO move
class NxVec3
{
public:
	NxVec3(const NxVec3& v);// public RVA = 0x1B5930
	void _CONSTRUCTOR(const NxVec3& v);// public RVA = 0x1B5930
	NxVec3(const float* v);// public RVA = 0x1BE630
	void _CONSTRUCTOR(const float* v);// public RVA = 0x1BE630
	// no_addr void NxVec3(const struct _Nx3F32 & _a1);// public missing arg names
	NxVec3(float _x, float _y, float _z);// public RVA = 0x1A6D00
	void _CONSTRUCTOR(float _x, float _y, float _z);// public RVA = 0x1A6D00
	NxVec3(float a);// public RVA = 0x7E7BA0
	void _CONSTRUCTOR(float a);// public RVA = 0x7E7BA0
	NxVec3() {};// public RVA = 0x1AA820
	void _CONSTRUCTOR();// public RVA = 0x1AA820
	// no_addr const class NxVec3 & operator=(const struct _Nx3F32 & _a1);// public missing arg names
	const NxVec3& operator=(const NxVec3& v);// public RVA = 0x1AA830
	// no_addr void get(double * _a1);// public missing arg names
	// no_addr void get(float * _a1);// public missing arg names
	// no_addr float * get();// public
	// no_addr const float * get();// public
	// no_addr float operator[](int _a1);// public missing arg names
	float& operator[](int index);// public RVA = 0x204170
	// no_addr bool operator<(const class NxVec3 & _a1);// public missing arg names
	bool operator==(const NxVec3& v) const;// public RVA = 0x1DE480
	bool operator!=(const NxVec3& v) const;// public RVA = 0x23DDB0
	// no_addr void set(float _a1);// public missing arg names
	void set(float _x, float _y, float _z);// public RVA = 0x1AA850
	// no_addr void set(const double * _a1);// public missing arg names
	void set(const float* v);// public RVA = 0x2455D0
	// no_addr void set(const class NxVec3 & _a1);// public missing arg names
	// no_addr void setx(const float & _a1);// public missing arg names
	// no_addr void sety(const float & _a1);// public missing arg names
	// no_addr void setz(const float & _a1);// public missing arg names
	// no_addr void setNegative();// public
	// no_addr void setNegative(const class NxVec3 & _a1);// public missing arg names
	void zero();// public RVA = 0x1AA870
	int isZero() const;// public RVA = 0x1AA7E0
	// no_addr void setPlusInfinity();// public
	// no_addr void setMinusInfinity();// public
	// no_addr void min(const class NxVec3 & _a1);// public missing arg names
	// no_addr void max(const class NxVec3 & _a1);// public missing arg names
	void add(const NxVec3& a, const NxVec3& b);// public RVA = 0x4EBDA0
	void subtract(const NxVec3& a, const NxVec3& b);// public RVA = 0x4EBDE0
	// no_addr void multiply(float _a1, const class NxVec3 & _a2);// public missing arg names
	// no_addr void arrayMultiply(const class NxVec3 & _a1, const class NxVec3 & _a2);// public missing arg names
	void multiplyAdd(float s, const NxVec3& a, const NxVec3& b);// public RVA = 0x4EBE20
	float normalize();// public RVA = 0x4CF520
	void setMagnitude(float length);// public RVA = 0x7D6B80
	unsigned int closestAxis() const;// public RVA = 0x204180
	// no_addr enum NxAxisType snapToClosestAxis();// public
	bool isFinite() const;// public RVA = 0x1AA880
	float dot(const NxVec3& v) const;// public RVA = 0x1AB190
	// no_addr bool sameDirection(const class NxVec3 & _a1);// public missing arg names
	float magnitude() const;// public RVA = 0x4CE950
	float magnitudeSquared() const;// public RVA = 0x1AB1C0
	// no_addr float distance(const class NxVec3 & _a1);// public missing arg names
	// no_addr float distanceSquared(const class NxVec3 & _a1);// public missing arg names
	// no_addr class NxVec3 cross(const class NxVec3 & _a1);// public missing arg names
	void cross(const NxVec3& left, const NxVec3& right);// public RVA = 0x1B5950
	// no_addr void setNotUsed();// public
	// no_addr int isNotUsed();// public
	bool equals(const NxVec3& v, float epsilon) const;// public RVA = 0x7E82E0
	// no_addr class NxVec3 operator-(const class NxVec3 & _a1);// public missing arg names
	NxVec3 operator-() const;// public RVA = 0x3D2660
	NxVec3 operator+(const NxVec3& v) const;// public RVA = 0x1BE650
	// no_addr class NxVec3 operator*(float _a1);// public missing arg names
	// no_addr class NxVec3 operator/(float _a1);// public missing arg names
	NxVec3& operator+=(const NxVec3& v);// public RVA = 0x1BE690
	NxVec3& operator-=(const NxVec3& v);// public RVA = 0x1D6EC0
	NxVec3& operator*=(float f);// public RVA = 0x1D6F00
	// no_addr class NxVec3 & operator/=(float _a1);// public missing arg names
	NxVec3 operator^(const NxVec3& v) const;// public RVA = 0x1B59E0
	// no_addr float operator|(const class NxVec3 & _a1);// public missing arg names
	float x; // 0x0 Member
	float y; // 0x4 Member
	float z; // 0x8 Member
};

namespace NXU
{
	class NxuPhysicsCollection;
	enum NXU_FileType
	{
		FT_BINARY = 0x80020008,
		FT_XML = 0x1,
		FT_COLLADA = 0x2
	};
	NxuPhysicsCollection* loadCollection(const char* fname, NXU::NXU_FileType type, void* mem, int len);// RVA = 0x1DD040
}
NXU::NxuPhysicsCollection* (*scaleCopyCollection)(const NXU::NxuPhysicsCollection* source, const char* newId, const NxVec3& scale, NxPhysicsSDK* sdk);// RVA = 0x20B820

boost::unordered_map<std::pair<std::string, int>, NXU::NxuPhysicsCollection*> physXCollectionCache;

NXU::NxuPhysicsCollection* (*loadPhysXResource_orig)(const std::string& filename, int type);// RVA = 0x7E4850
NXU::NxuPhysicsCollection* loadPhysXResource_hook(const std::string& filename, int type)
{
	if (Settings::GetCachePhysXColliders())
	{
		boost::unordered_map<std::pair<std::string, int>, NXU::NxuPhysicsCollection*>::iterator it = physXCollectionCache.find(std::make_pair(filename, type));
		NxVec3 scale;
		scale.x = 1;
		scale.y = 1;
		scale.z = 1;
		if (it == physXCollectionCache.end())
		{
			// I can't find a function for cloning a collection, so I create a scaled copy instead
			NXU::NxuPhysicsCollection* newCollection = loadPhysXResource_orig(filename, type);
			physXCollectionCache.emplace(std::make_pair(filename, type), scaleCopyCollection(newCollection, (filename + "_CLONE_RK").c_str(), scale, ((Kenshi::PhysicsActual*)Kenshi::GetGameWorld().physics)->physicsSDK));

			return newCollection;
		}
		return scaleCopyCollection(it->second, filename.c_str(), scale, ((Kenshi::PhysicsActual*)Kenshi::GetGameWorld().physics)->physicsSDK);
	}
	else
	{
		return loadPhysXResource_hook(filename, type);
	}
}

void PhysicsHooks::Init()
{
	loadPhysXResource_orig = Escort::JmpReplaceHook<NXU::NxuPhysicsCollection* (const std::string&, int)>(Kenshi::GetLoadPhysXResourceFunction(), loadPhysXResource_hook, 7);
	scaleCopyCollection = (NXU::NxuPhysicsCollection * (*)(const NXU::NxuPhysicsCollection*, const char*, const NxVec3&, NxPhysicsSDK*))Kenshi::GetScaleCopyCollectionFunction();
}