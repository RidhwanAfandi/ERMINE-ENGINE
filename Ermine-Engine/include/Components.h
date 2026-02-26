/* Start Header ************************************************************************/
/*!
\file       Components.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (45%)
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (10%)
\co-author  Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu (5%)
\co-author  WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu (40%)
\date       Jan 24, 2025
\brief      Updated components with modular material system

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "PreCompile.h"
#include "MathVector.h" // Vector3D included

//#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ScriptInstance.h"
#include "Texture.h"
#include "Material.h"
#include "AudioManager.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include "Model.h"
#include "shadow_config.h"
#include "Animator.h"
#include "AssetManager.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include "Guid.h"
#include "AnimationGUI.h"

#include "xcore/my_properties.h"
#include "xproperty.h"
#include "sprop/property_sprop.h"

#include "FSMNode.h"
#include "AABB.h"

namespace Ermine {
	// NOTE: ALL ENUMS TO BE ADDED UP HERE

	/*!***********************************************************************
	\brief
	 Meshkind type structure
	*************************************************************************/
	enum class MeshKind { None, Primitive/*, Asset*/ };

	/*!***********************************************************************
	\brief
	 Light type structure
	*************************************************************************/
	enum class LightType : int
	{
		POINT = 0,
		DIRECTIONAL = 1,
		SPOT = 2
	};

	/*!***********************************************************************
	 \brief
	 Enum for Physic component.
	*************************************************************************/
	enum class PhysicsBodyType
	{
		Rigid,
		Trigger
	};
	enum class ShapeType { Box, Sphere, Capsule, CustomMesh/*, Compound*/, Total };
}

namespace xproperty::settings {
	/*!***********************************************************************
	\brief
	 Meshkind type structure for xproperty
	*************************************************************************/
	template<>
	struct var_type<Ermine::MeshKind> : var_defaults<"MeshKind", Ermine::MeshKind>
	{
		inline static constexpr std::array enum_list_v{
			enum_item{"None",      Ermine::MeshKind::None},
			enum_item{"Primitive", Ermine::MeshKind::Primitive},
			//enum_item{"Asset",     Ermine::MeshKind::Asset},
		};
	};

	/*!***********************************************************************
	\brief
	 light type structure for xproperty
	*************************************************************************/
	template<>
	struct var_type<Ermine::LightType> : var_defaults<"LightType", Ermine::LightType>
	{
		// antlion: enum_item takes the enum, not integers
		inline static constexpr std::array enum_list_v{
			enum_item{"Point",       Ermine::LightType::POINT},
			enum_item{"Directional", Ermine::LightType::DIRECTIONAL},
			enum_item{"Spot",        Ermine::LightType::SPOT},
		};
	};

	/*!***********************************************************************
	\brief
	 Physics body type structure for xproperty
	*************************************************************************/
	template<> struct var_type<Ermine::PhysicsBodyType> : var_defaults<"PhysicsBodyType", Ermine::PhysicsBodyType> {
		inline static constexpr std::array enum_list_v{
			enum_item{"Rigid",   Ermine::PhysicsBodyType::Rigid},
			enum_item{"Trigger", Ermine::PhysicsBodyType::Trigger},
		};
	};

	/*!***********************************************************************
	\brief
	 JPH body type structure for xproperty
	*************************************************************************/
	template<> struct var_type<JPH::EMotionType> : var_defaults<"JPH_EMotionType", JPH::EMotionType> {
		inline static constexpr std::array enum_list_v{
			enum_item{"Static",    JPH::EMotionType::Static},
			enum_item{"Kinematic", JPH::EMotionType::Kinematic},
			enum_item{"Dynamic",   JPH::EMotionType::Dynamic},
		};
	};

	/*!***********************************************************************
	\brief
	 Shape type structure for xproperty
	*************************************************************************/
	template<> struct var_type<Ermine::ShapeType> : var_defaults<"ShapeType", Ermine::ShapeType> {
		inline static constexpr std::array enum_list_v{
			enum_item{"Box",        Ermine::ShapeType::Box},
			enum_item{"Sphere",     Ermine::ShapeType::Sphere},
			enum_item{"Capsule",    Ermine::ShapeType::Capsule},
			enum_item{"CustomMesh", Ermine::ShapeType::CustomMesh},
		};
	};
}


/*!***********************************************************************
\brief
 Helpers for xproperty
*************************************************************************/
namespace xprop_utils
{
	template<typename E>
	struct EnumMap; // specialize per-enum below

	template<typename E>
	constexpr const char* EnumToString(E e) {
		for (const auto& [name, val] : EnumMap<E>::items)
			if (val == e) return name;
		return nullptr;
	}

	template<typename E>
	inline bool StringToEnum(const char* s, E& out) {
		for (const auto& [name, val] : EnumMap<E>::items)
			if (std::strcmp(name, s) == 0) { out = val; return true; }
		return false;
	}

	// ---------- Enum maps (one-time specializations) ----------

	template<> struct EnumMap<Ermine::MeshKind> {
		using E = Ermine::MeshKind;
		static constexpr std::pair<const char*, E> items[] = {
			{"None",      E::None},
			{"Primitive", E::Primitive},
			//{"Asset",     E::Asset},
		};
	};
	constexpr std::pair<const char*, Ermine::MeshKind> EnumMap<Ermine::MeshKind>::items[];

	template<> struct EnumMap<Ermine::LightType> {
		using E = Ermine::LightType;
		static constexpr std::pair<const char*, E> items[] = {
			{"Point",       E::POINT},
			{"Directional", E::DIRECTIONAL},
			{"Spot",        E::SPOT},
		};
	};
	constexpr std::pair<const char*, Ermine::LightType> EnumMap<Ermine::LightType>::items[];

	template<> struct EnumMap<Ermine::PhysicsBodyType> {
		using E = Ermine::PhysicsBodyType;
		static constexpr std::pair<const char*, E> items[] = {
			{"Rigid",   E::Rigid},
			{"Trigger", E::Trigger},
		};
	};
	constexpr std::pair<const char*, Ermine::PhysicsBodyType> EnumMap<Ermine::PhysicsBodyType>::items[];

	template<> struct EnumMap<JPH::EMotionType> {
		using E = JPH::EMotionType;
		static constexpr std::pair<const char*, E> items[] = {
			{"Static",    E::Static},
			{"Kinematic", E::Kinematic},
			{"Dynamic",   E::Dynamic},
		};
	};
	constexpr std::pair<const char*, JPH::EMotionType> EnumMap<JPH::EMotionType>::items[];

	template<> struct EnumMap<Ermine::ShapeType> {
		using E = Ermine::ShapeType;
		static constexpr std::pair<const char*, E> items[] = {
			{"Box",        E::Box},
			{"Sphere",     E::Sphere},
			{"Capsule",    E::Capsule},
			{"CustomMesh", E::CustomMesh},
		};
	};
	constexpr std::pair<const char*, Ermine::ShapeType> EnumMap<Ermine::ShapeType>::items[];

	// Convert any xproperty-reflected object to JSON
	template<typename T, typename Alloc>
	void SerializeToJson(const T& obj, rapidjson::Value& out, Alloc& alloc)
	{
		out.SetObject();
		xproperty::settings::context ctx{};
		xproperty::sprop::container bag;
		xproperty::sprop::collector collect(obj, bag, ctx, true);

		for (auto& p : bag.m_Properties)
		{
			std::string key = p.m_Path.substr(p.m_Path.find_last_of('/') + 1);
			rapidjson::Value keyVal;
			keyVal.SetString(key.c_str(), (rapidjson::SizeType)key.size(), alloc);

			const auto guid = p.m_Value.getTypeGuid();

			// Serialize by GUID type
			if (guid == xproperty::settings::var_type<std::string>::guid_v)
			{
				const std::string& s = p.m_Value.get<std::string>();
				rapidjson::Value val;
				val.SetString(s.c_str(), (rapidjson::SizeType)s.size(), alloc);
				out.AddMember(keyVal, val, alloc);
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v)
				out.AddMember(keyVal, p.m_Value.get<bool>(), alloc);
			else if (guid == xproperty::settings::var_type<float>::guid_v)
				out.AddMember(keyVal, p.m_Value.get<float>(), alloc);
			else if (guid == xproperty::settings::var_type<int>::guid_v)
				out.AddMember(keyVal, p.m_Value.get<int>(), alloc);
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v)
				out.AddMember(keyVal, Vec3ToJson(p.m_Value.get<Ermine::Vec3>(), alloc), alloc);
			else if (guid == xproperty::settings::var_type<Ermine::Quaternion>::guid_v)
				out.AddMember(keyVal, QuatToJson(p.m_Value.get<Ermine::Quaternion>(), alloc), alloc);
			else if (guid == xproperty::settings::var_type<Ermine::Guid>::guid_v)
			{
				const Ermine::Guid& g = p.m_Value.get<Ermine::Guid>();
				std::string s = g.ToString(); // You already use Guid::ToString() in IDComponent

				rapidjson::Value val;
				val.SetString(s.c_str(),
					static_cast<rapidjson::SizeType>(s.size()),
					alloc);

				out.AddMember(keyVal, val, alloc);
			}
			else if (guid == xproperty::settings::var_type<Ermine::MeshKind>::guid_v) {
				if (const char* name = EnumToString(p.m_Value.get<Ermine::MeshKind>())) {
					rapidjson::Value val; val.SetString(name, (rapidjson::SizeType)std::strlen(name), alloc);
					out.AddMember(keyVal, val, alloc);
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::LightType>::guid_v) {
				if (const char* name = EnumToString(p.m_Value.get<Ermine::LightType>())) {
					rapidjson::Value val; val.SetString(name, (rapidjson::SizeType)std::strlen(name), alloc);
					out.AddMember(keyVal, val, alloc);
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::PhysicsBodyType>::guid_v) {
				if (const char* name = EnumToString(p.m_Value.get<Ermine::PhysicsBodyType>())) {
					rapidjson::Value val; val.SetString(name, (rapidjson::SizeType)std::strlen(name), alloc);
					out.AddMember(keyVal, val, alloc);
				}
			}
			else if (guid == xproperty::settings::var_type<JPH::EMotionType>::guid_v) {
				if (const char* name = EnumToString(p.m_Value.get<JPH::EMotionType>())) {
					rapidjson::Value val; val.SetString(name, (rapidjson::SizeType)std::strlen(name), alloc);
					out.AddMember(keyVal, val, alloc);
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::ShapeType>::guid_v) {
				if (const char* name = EnumToString(p.m_Value.get<Ermine::ShapeType>())) {
					rapidjson::Value val; val.SetString(name, (rapidjson::SizeType)std::strlen(name), alloc);
					out.AddMember(keyVal, val, alloc);
				}
			}
		}
	}

	// Deserialize back into any reflected type
	template<typename T>
	void DeserializeFromJson(T& obj, const rapidjson::Value& in)
	{
		if (!in.IsObject()) return;
		xproperty::settings::context ctx{};
		xproperty::sprop::container bag;
		xproperty::sprop::collector collect(obj, bag, ctx, true);
		std::string err;

		for (auto& p : bag.m_Properties)
		{
			std::string key = p.m_Path.substr(p.m_Path.find_last_of('/') + 1);
			if (!in.HasMember(key.c_str())) continue;
			const auto& v = in[key.c_str()];
			const auto guid = p.m_Value.getTypeGuid();

			if (guid == xproperty::settings::var_type<std::string>::guid_v && v.IsString())
				p.m_Value.set<std::string>(v.GetString());
			else if (guid == xproperty::settings::var_type<bool>::guid_v && v.IsBool())
				p.m_Value.set<bool>(v.GetBool());
			else if (guid == xproperty::settings::var_type<float>::guid_v && v.IsNumber())
				p.m_Value.set<float>(v.GetFloat());
			else if (guid == xproperty::settings::var_type<int>::guid_v && v.IsInt())
				p.m_Value.set<int>(v.GetInt());
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v && v.IsArray() && v.Size() == 3)
				p.m_Value.set<Ermine::Vec3>(Ermine::Vec3(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat()));
			else if (guid == xproperty::settings::var_type<Ermine::Quaternion>::guid_v && v.IsArray() && v.Size() == 4)
				p.m_Value.set<Ermine::Quaternion>(Ermine::Quaternion(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat(), v[3].GetFloat()));
			else if (guid == xproperty::settings::var_type<Ermine::Guid>::guid_v && v.IsString())
			{
				Ermine::Guid g = Ermine::Guid::FromString(v.GetString());
				p.m_Value.set<Ermine::Guid>(g);
			}
			else if (guid == xproperty::settings::var_type<Ermine::MeshKind>::guid_v) {
				Ermine::MeshKind tmp{};
				bool ok = v.IsString() ? StringToEnum(v.GetString(), tmp)
					: (v.IsInt() ? (tmp = static_cast<Ermine::MeshKind>(v.GetInt()), true) : false);
				if (ok) p.m_Value.set<Ermine::MeshKind>(tmp);
			}
			else if (guid == xproperty::settings::var_type<Ermine::LightType>::guid_v) {
				Ermine::LightType tmp{};
				bool ok = v.IsString() ? StringToEnum(v.GetString(), tmp)
					: (v.IsInt() ? (tmp = static_cast<Ermine::LightType>(v.GetInt()), true) : false);
				if (ok) p.m_Value.set<Ermine::LightType>(tmp);
			}
			else if (guid == xproperty::settings::var_type<Ermine::PhysicsBodyType>::guid_v) {
				Ermine::PhysicsBodyType tmp{};
				bool ok = v.IsString() ? StringToEnum(v.GetString(), tmp)
					: (v.IsInt() ? (tmp = static_cast<Ermine::PhysicsBodyType>(v.GetInt()), true) : false);
				if (ok) p.m_Value.set<Ermine::PhysicsBodyType>(tmp);
			}
			else if (guid == xproperty::settings::var_type<JPH::EMotionType>::guid_v) {
				JPH::EMotionType tmp{};
				bool ok = v.IsString() ? StringToEnum(v.GetString(), tmp)
					: (v.IsInt() ? (tmp = static_cast<JPH::EMotionType>(v.GetInt()), true) : false);
				if (ok) p.m_Value.set<JPH::EMotionType>(tmp);
			}
			else if (guid == xproperty::settings::var_type<Ermine::ShapeType>::guid_v) {
				Ermine::ShapeType tmp{};
				bool ok = v.IsString() ? StringToEnum(v.GetString(), tmp)
					: (v.IsInt() ? (tmp = static_cast<Ermine::ShapeType>(v.GetInt()), true) : false);
				if (ok) p.m_Value.set<Ermine::ShapeType>(tmp);
			}

			xproperty::sprop::setProperty(err, obj, p, ctx);
		}
	}
}

namespace Ermine
{
	template<typename Alloc>
	inline rapidjson::Value Vec2ToJson(const Ermine::Vec2& v, Alloc& a) {
		rapidjson::Value arr(rapidjson::kArrayType);
		arr.PushBack(v.x, a).PushBack(v.y, a);
		return arr;
	}

	inline Ermine::Vec2 JsonToVec2(const rapidjson::Value& v) {
		return Ermine::Vec2(v[0].GetFloat(), v[1].GetFloat());
	}

	inline Vec3 JsonToVec3(const rapidjson::Value& a) {
		return a.IsArray() && a.Size() == 3
			? Vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat())
			: Vec3();
	}

	inline rapidjson::Value Vec4ToJson(const Vec4& v, rapidjson::Document::AllocatorType& alloc) {
		rapidjson::Value a(rapidjson::kArrayType);
		a.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc).PushBack(v.w, alloc);
		return a;
	}

	inline Quaternion JsonToVec4(const rapidjson::Value& arr) {
		if (arr.IsArray() && arr.Size() == 4) {
			return Quaternion(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
		}
		return Quaternion(0.f, 0.f, 0.f, 0.f); // default fallback
	}

	inline rapidjson::Value Vec3ToJson(const Vec3& v, rapidjson::Document::AllocatorType& alloc) {
		rapidjson::Value a(rapidjson::kArrayType);
		a.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc);
		return a;
	}

	inline rapidjson::Value QuatToJson(const Quaternion& q, rapidjson::Document::AllocatorType& alloc) {
		rapidjson::Value a(rapidjson::kArrayType);
		a.PushBack(q.x, alloc).PushBack(q.y, alloc).PushBack(q.z, alloc).PushBack(q.w, alloc);
		return a;
	}

	struct IDComponent
	{
		Guid guid{};

		IDComponent() = default;
		explicit IDComponent(Guid g) : guid(g) {}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("guid", rapidjson::Value(guid.ToString().c_str(), alloc), alloc);
		}
		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("guid") && in["guid"].IsString()) {
				guid = Guid::FromString(in["guid"].GetString());
			}
		}
	};

	/*!***********************************************************************
	\brief
	 Transform component structure.
	*************************************************************************/
	struct Transform
	{
		//Mtx44 transform_matrix{ 1.0f }; // Local transform matrix
		Vec3 position;
		Quaternion rotation; // Euler angles in degrees
		Vec3 scale;
		bool isDirty{ true };

		explicit Transform(const Vec3& pos = Vec3(), const Quaternion& rot = Quaternion(), const Vec3& scl = Vec3(1.f, 1.f, 1.f))
			: position(pos), rotation(rot), scale(scl)
		{
		}

		// Helper method to build local transform matrix
		Mtx44 GetLocalMatrix() const
		{
			Mtx44 translation, rotation_mtx, scale_mtx;
			Mtx44Identity(translation);
			Mtx44Identity(rotation_mtx);
			Mtx44Identity(scale_mtx);

			// Set translation - this overwrites the translation part of identity matrix
			translation.m03 = position.x;
			translation.m13 = position.y;
			translation.m23 = position.z;

			// Set rotation from quaternion
			Mtx44SetFromQuaternion(rotation_mtx, rotation);

			// Set scale - this overwrites the scale part of identity matrix
			scale_mtx.m00 = scale.x;
			scale_mtx.m11 = scale.y;
			scale_mtx.m22 = scale.z;

			// Correct multiplication order - Scale -> Rotate -> Translate (SRT)
			return translation * rotation_mtx * scale_mtx;
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"Transform", Transform,
			xproperty::obj_member<"position", &Transform::position>,
			xproperty::obj_member<"rotation", &Transform::rotation>,
			xproperty::obj_member<"scale", &Transform::scale>
		);
	};

	//XPROPERTY_REG(Transform);

	/*!***********************************************************************
	\brief
	 GlobalTransform component - WORLD TRANSFORM FOR RENDERING.
	*************************************************************************/
	struct GlobalTransform
	{
		Mtx44 worldMatrix{ 1.0f };  // World transform matrix used by renderer
		bool isDirty{ true };

		GlobalTransform() = default;

		explicit GlobalTransform(const Mtx44& matrix) : worldMatrix(matrix), isDirty(false) {}

		// Extract world position from matrix
		Vec3 GetWorldPosition() const
		{
			return Vec3(worldMatrix.m03, worldMatrix.m13, worldMatrix.m23);
		}

		// Extract world rotation from matrix
		Quaternion GetWorldRotation() const
		{
			// Remove translation and scale to get rotation matrix
			Mtx44 rotMatrix = worldMatrix;
			rotMatrix.m03 = 0.0f; rotMatrix.m13 = 0.0f; rotMatrix.m23 = 0.0f; rotMatrix.m33 = 1.0f;

			// Remove scale
			Vec3 xAxis(rotMatrix.m00, rotMatrix.m10, rotMatrix.m20);
			Vec3 yAxis(rotMatrix.m01, rotMatrix.m11, rotMatrix.m21);
			Vec3 zAxis(rotMatrix.m02, rotMatrix.m12, rotMatrix.m22);

			float xLen = sqrtf(xAxis.x * xAxis.x + xAxis.y * xAxis.y + xAxis.z * xAxis.z);
			float yLen = sqrtf(yAxis.x * yAxis.x + yAxis.y * yAxis.y + yAxis.z * yAxis.z);
			float zLen = sqrtf(zAxis.x * zAxis.x + zAxis.y * zAxis.y + zAxis.z * zAxis.z);

			if (xLen > 0.0f) { xAxis.x /= xLen; xAxis.y /= xLen; xAxis.z /= xLen; }
			if (yLen > 0.0f) { yAxis.x /= yLen; yAxis.y /= yLen; yAxis.z /= yLen; }
			if (zLen > 0.0f) { zAxis.x /= zLen; zAxis.y /= zLen; zAxis.z /= zLen; }

			rotMatrix.m00 = xAxis.x; rotMatrix.m10 = xAxis.y; rotMatrix.m20 = xAxis.z;
			rotMatrix.m01 = yAxis.x; rotMatrix.m11 = yAxis.y; rotMatrix.m21 = yAxis.z;
			rotMatrix.m02 = zAxis.x; rotMatrix.m12 = zAxis.y; rotMatrix.m22 = zAxis.z;

			return Mtx44GetQuaternion(rotMatrix);
		}

		// Extract world scale from matrix
		Vec3 GetWorldScale() const
		{
			Vec3 xAxis(worldMatrix.m00, worldMatrix.m10, worldMatrix.m20);
			Vec3 yAxis(worldMatrix.m01, worldMatrix.m11, worldMatrix.m21);
			Vec3 zAxis(worldMatrix.m02, worldMatrix.m12, worldMatrix.m22);

			return Vec3(
				sqrtf(xAxis.x * xAxis.x + xAxis.y * xAxis.y + xAxis.z * xAxis.z),
				sqrtf(yAxis.x * yAxis.x + yAxis.y * yAxis.y + yAxis.z * yAxis.z),
				sqrtf(zAxis.x * zAxis.x + zAxis.y * zAxis.y + zAxis.z * zAxis.z)
			);
		}

		Vec3 GetWorldForward() const {
			// Column 2 of worldMatrix (assuming column-major and +Z forward)
			Vec3 f(worldMatrix.m02, worldMatrix.m12, worldMatrix.m22);
			float len = Vec3Length(f);
			return (len > 0.0f) ? f / len : Vec3(0.f, 0.f, 1.f);
		}
		Vec3 GetWorldRight() const {
			Vec3 r(worldMatrix.m00, worldMatrix.m10, worldMatrix.m20);
			float len = Vec3Length(r);
			return (len > 0.0f) ? r / len : Vec3(1.f, 0.f, 0.f);
		}
		Vec3 GetWorldUp() const {
			Vec3 u(worldMatrix.m01, worldMatrix.m11, worldMatrix.m21);
			float len = Vec3Length(u);
			return (len > 0.0f) ? u / len : Vec3(0.f, 1.f, 0.f);
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			(void)alloc;
			out.SetObject();
			// Don't serialize the matrix - it gets recalculated from hierarchy
		}

		void Deserialize(const rapidjson::Value& in) {
			(void)in;
			// Don't deserialize the matrix - it gets recalculated from hierarchy
			isDirty = true;
		}

		XPROPERTY_DEF(
			"GlobalTransform", GlobalTransform,
			xproperty::obj_member<"isDirty", &GlobalTransform::isDirty>
		);
	};

	/*!***********************************************************************
	\brief
	 Rigidbody3D component structure.
	*************************************************************************/
	struct Rigidbody3D
	{
		// Linear Properties
		Vec3 position{};
		Vec3 velocity{};
		Vec3 acceleration{};
		Vec3 force{};
		float mass{ 1.f };				// Minimum mass of 1
		float inverse_mass{ 1.f / mass }; // inverse mass = 1/mass
		float linear_drag{ 0.9f };		// Adjust to control the friction from [0, 1]

		// Rotational Properties
		float angle{};
		float angular_velocity{};
		float angular_acceleration{};
		float torque{};
		float inertia_mass{ 1.f };					// Minimum inertia mass of 1
		float inv_inertia_mass{ 1.f / inertia_mass }; // inverse inertia mass = 1/inertia mass
		float angular_drag{ 0.9f };					// Adjust to control the friction from [0, 1]

		bool use_gravity{ false };  // If true, apply gravity
		bool is_kinematic{ false }; // If true, don't apply physics

		explicit Rigidbody3D(const Vec3& pos = Vec3(), const Vec3& vel = Vec3(), const Vec3& acc = Vec3(), const Vec3& frc = Vec3(), float m = 1.f, float inv_m = 1.f, float lin_drag = 0.9f,
			float ang_drag = 0.9f, bool use_grav = false, bool is_kinem = false) :
			position(pos), velocity(vel), acceleration(acc), force(frc), mass(m), inverse_mass(inv_m), linear_drag(lin_drag), angular_drag(ang_drag), use_gravity(use_grav), is_kinematic(is_kinem)
		{
		}

		//
	};

	/*!***********************************************************************
	\brief
	 Object Meta Data structure
	*************************************************************************/
	struct ObjectMetaData
	{
		std::string name{};
		std::string tag{};

		bool selfActive{ };

		ObjectMetaData() : name("GameObject"), tag("Untagged"), selfActive(true)
		{
		}

		ObjectMetaData(std::string name_, std::string tag_, const bool& active) : name(std::move(name_)), tag(std::move(
			tag_)), selfActive(active)
		{
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"ObjectMetaData", ObjectMetaData,
			xproperty::obj_member<"name", &ObjectMetaData::name>,
			xproperty::obj_member<"tag", &ObjectMetaData::tag>,
			xproperty::obj_member<"selfActive", &ObjectMetaData::selfActive>
		)
	};

	/*!***********************************************************************
	 \brief
	 Script field value structure for storing different types of script variables
	 *************************************************************************/
	struct ScriptFieldValue
	{
		enum class Kind { Float = 0, Int = 1, Bool = 2, String = 3, Vector3 = 4, Quaternion = 5 };

		using Value = std::variant<float, int, bool, std::string, Vec3, Quaternion>;

		Kind  kind = Kind::Float;
		Value value = 0.0f;

		static ScriptFieldValue MakeFloat(float v)
		{
			ScriptFieldValue r;
			r.kind = Kind::Float;
			r.value = v;
			return r;
		}

		static ScriptFieldValue MakeInt(int v)
		{
			ScriptFieldValue r;
			r.kind = Kind::Int;
			r.value = v;
			return r;
		}

		static ScriptFieldValue MakeBool(bool v)
		{
			ScriptFieldValue r;
			r.kind = Kind::Bool;
			r.value = v;
			return r;
		}

		static ScriptFieldValue MakeString(std::string v)
		{
			ScriptFieldValue r;
			r.kind = Kind::String;
			r.value = std::move(v);
			return r;
		}

		static ScriptFieldValue MakeVec3(const Vec3& v)
		{
			ScriptFieldValue r;
			r.kind = Kind::Vector3;
			r.value = v;
			return r;
		}

		static ScriptFieldValue MakeQuat(const Quaternion& v)
		{
			ScriptFieldValue r;
			r.kind = Kind::Quaternion;
			r.value = v;
			return r;
		}
	};

	/*!***********************************************************************
	\brief
	 Script structure
	*************************************************************************/
	struct Script
	{
		std::string m_className;
		std::unique_ptr<scripting::ScriptInstance> m_instance;
		bool m_enabled = true;
		bool m_started = false;

		std::unordered_map<std::string, ScriptFieldValue> m_fields;

		Script() = default;
		explicit Script(std::string className, EntityID id) : m_className(std::move(className))
		{
			auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", m_className));
			m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), id);
		}

		Script(const Script& other) : m_className(other.m_className), m_fields(other.m_fields)
		{
			if (other.m_instance)
			{
				// Re-create the script instance with the same class
				auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", m_className));
				m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), other.m_instance->entityID);
			}
		}

		Script& operator=(const Script& other)
		{
			if (this != &other)
			{
				m_className = other.m_className;
				m_fields = other.m_fields;
				if (other.m_instance)
				{
					// Re-create the script instance with the same class
					auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", m_className));
					m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), other.m_instance->entityID);
				}
				else
				{
					m_instance.reset();
				}
			}
			return *this;
		}

		Script(Script&& other) noexcept : m_className(std::move(other.m_className)),
			m_instance(std::move(other.m_instance)), m_fields(std::move(other.m_fields))
		{
		}

		Script& operator=(Script&& other) noexcept
		{
			if (this != &other)
			{
				m_className = std::move(other.m_className);
				m_instance = std::move(other.m_instance);
				m_fields = std::move(other.m_fields);
			}
			return *this;
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("class", rapidjson::Value(m_className.c_str(), alloc), alloc);
			out.AddMember("enabled", m_enabled, alloc);

			rapidjson::Value fields(rapidjson::kObjectType);
			for (const auto& kv : m_fields)
			{
				rapidjson::Value fld(rapidjson::kObjectType);
				fld.AddMember("t", static_cast<int>(kv.second.kind), alloc);
				try
				{
					switch (kv.second.kind)
					{
					case ScriptFieldValue::Kind::Float:
						fld.AddMember("v", std::get<float>(kv.second.value), alloc); break;
					case ScriptFieldValue::Kind::Int:
						fld.AddMember("v", std::get<int>(kv.second.value), alloc); break;
					case ScriptFieldValue::Kind::Bool:
						fld.AddMember("v", std::get<bool>(kv.second.value), alloc); break;
					case ScriptFieldValue::Kind::String: {
						const std::string& s = std::get<std::string>(kv.second.value);
						rapidjson::Value vs;
						vs.SetString(s.c_str(),
							static_cast<rapidjson::SizeType>(s.size()),
							alloc);
						fld.AddMember(rapidjson::StringRef("v"), vs, alloc);
						break;
					}
					case ScriptFieldValue::Kind::Vector3:
						fld.AddMember("v", Vec3ToJson(std::get<Vec3>(kv.second.value), alloc), alloc); break;
					case ScriptFieldValue::Kind::Quaternion:
						fld.AddMember("v", QuatToJson(std::get<Quaternion>(kv.second.value), alloc), alloc); break;
					default: break;
					}
				}
				catch (const std::bad_variant_access& ex)
				{
					// Skip invalid variant access
					EE_CORE_WARN(ex.what());
					continue;
				}

				fields.AddMember(rapidjson::Value(kv.first.c_str(), alloc), fld, alloc);
			}
			out.AddMember("fields", fields, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("class") && in["class"].IsString()) m_className = in["class"].GetString();
			if (in.HasMember("enabled") && in["enabled"].IsBool()) m_enabled = in["enabled"].GetBool();
			// Note: re-create ScriptInstance when attaching to entity (needs EntityID)

			m_fields.clear();
			if (in.HasMember("fields") && in["fields"].IsObject())
			{
				for (auto it = in["fields"].MemberBegin(); it != in["fields"].MemberEnd(); ++it)
				{
					const std::string name = it->name.GetString();
					const auto& fld = it->value;
					if (!fld.IsObject() || !fld.HasMember("t") || !fld.HasMember("v")) continue;

					const int t = fld["t"].GetInt();
					switch (static_cast<ScriptFieldValue::Kind>(t))
					{
					case ScriptFieldValue::Kind::Float:
						if (fld["v"].IsNumber()) m_fields[name] = ScriptFieldValue::MakeFloat(fld["v"].GetFloat());
						break;
					case ScriptFieldValue::Kind::Int:
						if (fld["v"].IsInt()) m_fields[name] = ScriptFieldValue::MakeInt(fld["v"].GetInt());
						break;
					case ScriptFieldValue::Kind::Bool:
						if (fld["v"].IsBool()) m_fields[name] = ScriptFieldValue::MakeBool(fld["v"].GetBool());
						break;
					case ScriptFieldValue::Kind::String:
						if (fld["v"].IsString()) m_fields[name] = ScriptFieldValue::MakeString(fld["v"].GetString());
						break;
					case ScriptFieldValue::Kind::Vector3:
						if (fld["v"].IsArray()) m_fields[name] = ScriptFieldValue::MakeVec3(JsonToVec3(fld["v"]));
						break;
					case ScriptFieldValue::Kind::Quaternion:
						if (fld["v"].IsArray()) m_fields[name] = ScriptFieldValue::MakeQuat(JsonToVec4(fld["v"]));
						break;
					default: break;
					}
				}
			}
		}

		XPROPERTY_DEF(
			"Script", Script,
			xproperty::obj_member<"class", &Script::m_className>,
			xproperty::obj_member<"enabled", &Script::m_enabled>
		)
	};

	struct ScriptsComponent
	{
		std::vector<Script> scripts;

		void AttachAll(EntityID id)
		{
			for (auto& s : scripts)
			{
				if (!s.m_instance)
				{
					auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", s.m_className));
					s.m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), id);
					// Match enabled state so OnEnable is invoked appropriately
					s.m_instance->SetEnabled(s.m_enabled);
					s.m_started = false;
					if(s.m_fields.size() > 0)
						scripting::ScriptEngine::PushCacheToManagedFields(s.m_instance->GetManaged(), s.m_fields);
				}
			}
		}
		
		//void AttachAll(EntityID id, const std::unordered_map<std::string, ScriptFieldValue>& cache)
		//{
		//	for (auto& s : scripts)
		//	{
		//		if (!s.m_instance)
		//		{
		//			auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", s.m_className));
		//			s.m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), id);
		//			// Match enabled state so OnEnable is invoked appropriately
		//			s.m_instance->SetEnabled(s.m_enabled);
		//			s.m_started = false;
		//			scripting::ScriptEngine::PushCacheToManagedFields(s.m_instance->object, cache);
		//		}
		//	}
		//}

		// Add a new script by class name (instantiates immediately for the given entity)
		void Add(const std::string& className, EntityID id, bool enabled = true)
		{
			Script s;
			s.m_className = className;
			s.m_enabled = enabled;
			auto sc = std::make_unique<scripting::ScriptClass>(scripting::ScriptClass("", s.m_className));
			s.m_instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), id);
			s.m_instance->SetEnabled(enabled);
			scripts.emplace_back(std::move(s));
		}

		void AddEmpty()
		{
			Script s;
			scripts.emplace_back(std::move(s));
		}

		// Remove first script matching class name
		bool RemoveByClass(const std::string& className)
		{
			auto it = std::find_if(scripts.begin(), scripts.end(),
				[&](const Script& s) { return s.m_className == className; });
			if (it == scripts.end()) return false;
			scripts.erase(it);
			return true;
		}

		Script& GetByClass(const std::string& className)
		{
			auto it = ranges::find_if(scripts,
			                          [&](const Script& s) { return s.m_className == className; });
			if (it == scripts.end())
			{
				throw std::runtime_error("Script class not found: " + className);
			}
			return *it;
		}

		// Serialize as an array of Script objects
		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			rapidjson::Value arr(rapidjson::kArrayType);
			for (const auto& s : scripts)
			{
				rapidjson::Value js(rapidjson::kObjectType);
				s.Serialize(js, alloc);
				arr.PushBack(js, alloc);
			}
			out.AddMember("scripts", arr, alloc);
		}

		// Deserialize Script authoring data only (instances are created later with AttachAll/Add)
		void Deserialize(const rapidjson::Value& in)
		{
			scripts.clear();
			if (!in.IsObject() || !in.HasMember("scripts") || !in["scripts"].IsArray())
				return;

			for (const auto& v : in["scripts"].GetArray())
			{
				if (!v.IsObject()) continue;
				Script s;
				s.Deserialize(v);
				// Do not create ScriptInstance here (no EntityID yet) - ScriptSystem should call AttachAll
				scripts.emplace_back(std::move(s));
			}
		}

		// Optional reflection stub (no per-field reflection for vectors)
		XPROPERTY_DEF("ScriptsComponent", ScriptsComponent)
	};

	/*!***********************************************************************
	\brief
	 Camera component structure
	*************************************************************************/
	struct CameraComponent
	{
		float fov;
		float aspectRatio;
		float nearPlane;
		float farPlane;
		bool isPrimary; // Is this the main camera?
		bool isGameCamera; // Is this a first-person game camera (vs editor camera)?

		explicit CameraComponent(float fov_ = 45.0f, float aspect = 16.0f / 9.0f, float nearP = 0.1f, float farP = 100.0f,
			bool primary = false, bool gameCamera = false) :
			fov(fov_), aspectRatio(aspect), nearPlane(nearP), farPlane(farP),
			isPrimary(primary), isGameCamera(gameCamera)
		{
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("fov", fov, alloc);
			out.AddMember("aspect", aspectRatio, alloc);
			out.AddMember("near", nearPlane, alloc);
			out.AddMember("far", farPlane, alloc);
			out.AddMember("primary", isPrimary, alloc);
			out.AddMember("isGameCamera", isGameCamera, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("fov")) fov = in["fov"].GetFloat();
			if (in.HasMember("aspect")) aspectRatio = in["aspect"].GetFloat();
			if (in.HasMember("near")) nearPlane = in["near"].GetFloat();
			if (in.HasMember("far")) farPlane = in["far"].GetFloat();
			if (in.HasMember("primary")) isPrimary = in["primary"].GetBool();
			if (in.HasMember("isGameCamera")) isGameCamera = in["isGameCamera"].GetBool();
		}

		XPROPERTY_DEF(
			"CameraComponent", CameraComponent,
			xproperty::obj_member<"fov", &CameraComponent::fov>,
			xproperty::obj_member<"aspectRatio", &CameraComponent::aspectRatio>,
			xproperty::obj_member<"nearPlane", &CameraComponent::nearPlane>,
			xproperty::obj_member<"farPlane", &CameraComponent::farPlane>,
			xproperty::obj_member<"isPrimary", &CameraComponent::isPrimary>,
			xproperty::obj_member<"isGameCamera", &CameraComponent::isGameCamera>
		)
	};

	struct MeshPrimitiveDesc {
		std::string type;
		Vec3        size{ 1,1,1 };

		XPROPERTY_DEF(
			"MeshPrimitiveDesc", MeshPrimitiveDesc,
			xproperty::obj_member<"type", &MeshPrimitiveDesc::type>,
			xproperty::obj_member<"size", &MeshPrimitiveDesc::size>
		)
	};

	struct MeshAssetDesc {
		std::string meshName;

		XPROPERTY_DEF(
			"MeshAssetDesc", MeshAssetDesc,
			xproperty::obj_member<"meshName", &MeshAssetDesc::meshName>
		)
	};
}

namespace Ermine
{
	/*!***********************************************************************
	\brief
	 Mesh structure
	*************************************************************************/
	struct Mesh
	{
		std::shared_ptr<graphics::VertexArray> vertex_array;
		std::shared_ptr<graphics::VertexBuffer> vertex_buffer;
		std::shared_ptr<graphics::IndexBuffer> index_buffer;

		MeshKind        kind = MeshKind::None;
		MeshPrimitiveDesc primitive;
		MeshAssetDesc     asset;
		std::string registeredMeshID; // Mesh ID registered in MeshManager

		// AABB for frustum culling (in local/model space)
		Vec3 aabbMin{ -1.0f, -1.0f, -1.0f };
		Vec3 aabbMax{ 1.0f,  1.0f,  1.0f };

		//Physic mesh collider
		std::vector<glm::vec3> cpuVertices;

		Mesh() = default;

		Mesh(const std::shared_ptr<graphics::VertexArray>& vao, const std::shared_ptr<graphics::VertexBuffer>& vbo, const std::shared_ptr<graphics::IndexBuffer>& ibo) :
			vertex_array(vao), vertex_buffer(vbo), index_buffer(ibo)
		{
		}

		void RebuildPrimitive(); // implemented in .cpp
		//void Deserialize(const rapidjson::Value& in) {
		//	// ... set kind/primitive.type/primitive.size as you already do ...
		//	if (kind == Kind::Primitive) RebuildPrimitive();
		//	// Asset case: your loader, etc.
		//}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			switch (kind) {
			case MeshKind::Primitive: {
				out.AddMember("kind", "Primitive", alloc);
				rapidjson::Value p(rapidjson::kObjectType);
				p.AddMember("type", rapidjson::Value(primitive.type.c_str(), alloc), alloc);
				p.AddMember("size", Vec3ToJson(primitive.size, alloc), alloc);
				out.AddMember("primitive", p, alloc);
				break;
			}
			//case MeshKind::Asset: {
			//	out.AddMember("kind", "Asset", alloc);
			//	rapidjson::Value a(rapidjson::kObjectType);
			//	a.AddMember("meshName", rapidjson::Value(asset.meshName.c_str(), alloc), alloc);
			//	out.AddMember("asset", a, alloc);
			//	break;
			//}
			default:
				out.AddMember("kind", "None", alloc);
				break;
			}
		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.HasMember("kind")) { kind = MeshKind::None; return; }
			const auto& k = in["kind"];
			if (k.IsString()) {
				if (strcmp(k.GetString(), "Primitive") == 0) {
					kind = MeshKind::Primitive;
					const auto& p = in["primitive"];
					if (p.HasMember("type") && p["type"].IsString()) primitive.type = p["type"].GetString();
					if (p.HasMember("size")) primitive.size = JsonToVec3(p["size"]);

					RebuildPrimitive();
					// extend for other primitives
				}

				//else if (strcmp(k.GetString(), "Asset") == 0) {
				//	kind = MeshKind::Asset;
				//	const auto& a = in["asset"];
				//	if (a.HasMember("meshName") && a["meshName"].IsString()) asset.meshName = a["meshName"].GetString();
				//	// Rebuild from asset if you have a mesh loader separate from Model
				//}
				else {
					kind = MeshKind::None;
				}
			}
		}

		// XPROPERTY_DEF
		XPROPERTY_DEF(
			"Mesh", Mesh,
			xproperty::obj_member<"kind", &Mesh::kind>,
			xproperty::obj_member<"primitive", &Mesh::primitive>
			//xproperty::obj_member<"asset", &Mesh::asset>
		)
	};

	/*!***********************************************************************
	\brief
	 Material component structure
	*************************************************************************/
	struct Material
	{
		//material class
		mutable std::shared_ptr<graphics::Material> m_material;
		Guid materialGuid{};

		// ---- Minimal cached authoring values for serialization ----
		std::string materialTemplate;       // optional
		bool hasAlbedo = false;   Vec3  cacheAlbedo{ 1,1,1 };
		bool hasRough = false;   float cacheRoughness = 0.5f;
		bool hasMetal = false;   float cacheMetallic = 0.0f;
		bool hasEmiss = false;   Vec3  cacheEmissive{ 0,0,0 };
		float cacheEmissiveIntensity = 1.0f;
		std::string customFragmentShader = "";   // Custom fragment shader path (empty = use standard PBR)
		bool cacheCastsShadows = true;           // Whether this material casts shadows

		//// Cached texture paths (only what we set by path)
		//bool hasAlbedoMapPath = false;   std::string albedoMapPath;
		//bool hasNormalMapPath = false;   std::string normalMapPath;

		Material() = default;

		/**
		 * @brief Constructor taking a modular material.
		 * @param material A shared pointer to a `graphics::Material` object that will be used to initialize the Material.
		 */
		Material(std::shared_ptr<graphics::Material> material, Guid guid = {}) : m_material(std::move(material)), materialGuid(guid)
		{
		}

		/**
		 * @brief Legacy constructor for backwards compatibility.
		 * @param shader The shader to associate with the material.
		 * @param texture The texture to associate with the material (optional). If valid, it is set as the albedo map only.
		 */
		Material(const std::shared_ptr<graphics::Shader>& shader, const std::shared_ptr<graphics::Texture>& texture)
		{
			m_material = std::make_shared<graphics::Material>(shader);

			// Only set texture and flags if texture is explicitly provided and valid
			if (texture && texture->IsValid())
			{
				m_material->SetTexture("materialAlbedoMap", texture);
				m_material->SetBool("materialHasAlbedoMap", true);
			}

			// Set default PBR values without any texture assumptions
			m_material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
		}

		/**
		 * @brief Copy constructor for the Material class.
		 * @param other The other Material object to copy from.
		 */
		Material(const Material& other) : m_material(other.m_material), materialGuid(other.materialGuid)
		{
			// Shared ownership - multiple entities can share the same material
		}

		/**
		 * @brief Copy assignment operator for the Material class.
		 * @param other The other Material object to copy from.
		 * @return A reference to this Material object after the copy assignment.
		 */
		Material& operator=(const Material& other)
		{
			if (this != &other)
			{
				m_material = other.m_material; // Shared ownership
				materialGuid = other.materialGuid;
			}
			return *this;
		}

		/**
		 * @brief Move constructor for the Material class.
		 * @param other The Material object to move from.
		 */
		Material(Material&& other) noexcept : m_material(std::move(other.m_material)), materialGuid(other.materialGuid)
		{
		}

		/**
		 * @brief Move assignment operator for the Material class.
		 * @param other The Material object to move from.
		 * @return A reference to this Material object after the move assignment.
		 */
		Material& operator=(Material&& other) noexcept
		{
			if (this != &other)
			{
				m_material = std::move(other.m_material);
				materialGuid = other.materialGuid;
			}
			return *this;
		}

		/**
		 * @brief Retrieves the raw `graphics::Material` pointer.
		 * @return A pointer to the internal `graphics::Material` object.
		 */
		graphics::Material* GetMaterial() const {
			if (!m_material && materialGuid.IsValid()) {
				m_material = AssetManager::GetInstance().GetMaterialByGuid(materialGuid);
			}
			return m_material.get();
		}

		/**
		 * @brief Get the shared material pointer for sharing between entities.
		 * @return A shared pointer to the internal `graphics::Material` object.
		 */
		std::shared_ptr<graphics::Material> GetSharedMaterial() const {
			if (!m_material && materialGuid.IsValid()) {
				m_material = AssetManager::GetInstance().GetMaterialByGuid(materialGuid);
			}
			return m_material;
		}

		void SetMaterial(const std::shared_ptr<graphics::Material>& material, Guid guid) {
			m_material = material;
			materialGuid = guid;
		}

		/**
		* @brief Sets the albedo color for the material.
		* @details Albedo represents the diffuse color of the material.
		* @param albedo A Vec3 representing the RGB color value for the albedo.
		*/
		void SetAlbedo(const Vec3& albedo)
		{
			hasAlbedo = true; cacheAlbedo = albedo;
			if (m_material) m_material->SetVec3("material.albedo", albedo);
		}

		/**
		* @brief Sets the roughness value for the material.
		* @details Roughness defines the material's surface smoothness. A value of 0.0 is smooth, and 1.0 is rough.
		* @param roughness A float representing the roughness of the material.
		*/
		void SetRoughness(float roughness)
		{
			hasRough = true; cacheRoughness = roughness;
			if (m_material) m_material->SetFloat("material.roughness", roughness);
		}

		/**
		* @brief Sets the metallic value for the material.
		* @details Metallic defines whether the material is metallic or dielectric. A value of 1.0 means fully metallic.
		* @param metallic A float representing the metallic property of the material.
		*/
		void SetMetallic(float metallic)
		{
			hasMetal = true; cacheMetallic = metallic;
			if (m_material) m_material->SetFloat("material.metallic", metallic);
		}

		/**
		* @brief Sets the emissive color for the material.
		* @details Emissive represents the material's self-illumination. It can be used to simulate glowing materials.
		* @param emissive A Vec3 representing the RGB color of the emissive property.
		* @param intensity A float value controlling the intensity of the emissive property (default: 1.0).
		*/
		void SetEmissive(const Vec3& emissive, float intensity = 1.0f)
		{
			hasEmiss = true; cacheEmissive = emissive; cacheEmissiveIntensity = intensity;
			if (m_material)
			{
				m_material->SetVec3("material.emissive", emissive);
				m_material->SetFloat("material.emissiveIntensity", intensity);
			}
		}

		/**
		* @brief Sets the normal map for the material.
		* @details The normal map is used to simulate small surface details like bumps and dents.
		* @param normalMap Shared pointer to a valid Texture representing the normal map.
		*/
		void SetNormalMap(std::shared_ptr<graphics::Texture> normalMap)
		{
			if (m_material && normalMap && normalMap->IsValid())
			{
				m_material->SetTexture("material.normalMap", normalMap);
				m_material->SetBool("material.hasNormalMap", true);
			}
		}

		/**
		* @brief Syncs cached values from the internal material for serialization.
		* @details Call this before saving to ensure cached values match the actual material state.
		*/
		void SyncFromMaterial()
		{
			if (!m_material) return;

			// Sync albedo
			if (auto param = m_material->GetParameter("materialAlbedo"))
			{
				if (param->floatValues.size() >= 3)
				{
					hasAlbedo = true;
					cacheAlbedo = Vec3(param->floatValues[0], param->floatValues[1], param->floatValues[2]);
				}
			}

			// Sync roughness
			if (auto param = m_material->GetParameter("materialRoughness"))
			{
				if (!param->floatValues.empty())
				{
					hasRough = true;
					cacheRoughness = param->floatValues[0];
				}
			}

			// Sync metallic
			if (auto param = m_material->GetParameter("materialMetallic"))
			{
				if (!param->floatValues.empty())
				{
					hasMetal = true;
					cacheMetallic = param->floatValues[0];
				}
			}

			// Sync emissive
			if (auto param = m_material->GetParameter("materialEmissive"))
			{
				if (param->floatValues.size() >= 3)
				{
					hasEmiss = true;
					cacheEmissive = Vec3(param->floatValues[0], param->floatValues[1], param->floatValues[2]);
				}
			}

			if (auto param = m_material->GetParameter("materialEmissiveIntensity"))
			{
				if (!param->floatValues.empty())
				{
					cacheEmissiveIntensity = param->floatValues[0];
				}
			}

			// Sync shadows
			if (auto param = m_material->GetParameter("materialCastsShadows"))
			{
				cacheCastsShadows = param->boolValue;
			}
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			Guid guidToWrite = materialGuid;
			if (!guidToWrite.IsValid() && m_material) {
				auto& assets = AssetManager::GetInstance();
				guidToWrite = assets.FindMaterialGuid(m_material.get());
				if (!guidToWrite.IsValid()) {
					std::string fallbackName = "Material_" + Guid::New().ToString();
					guidToWrite = assets.SaveMaterialAsset(fallbackName, *m_material, true, customFragmentShader);
				}
				const_cast<Material*>(this)->materialGuid = guidToWrite;
			}

			std::string guidStr = guidToWrite.IsValid() ? guidToWrite.ToString() : "";
			out.AddMember("guid",
				rapidjson::Value(guidStr.c_str(), (rapidjson::SizeType)guidStr.size(), alloc),
				alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.IsObject()) return;
			if (in.HasMember("guid") && in["guid"].IsString()) {
				std::string guidStr = in["guid"].GetString();
				if (!guidStr.empty()) {
					materialGuid = Guid::FromString(guidStr);
					m_material = AssetManager::GetInstance().GetMaterialByGuid(materialGuid);
					if (!m_material) {
						materialGuid = {};
						m_material.reset();
					}
				}
				else {
					materialGuid = {};
					m_material.reset();
				}
				return;
			}

			// Ensure material exists
			if (!m_material) {
				m_material = std::make_shared<graphics::Material>();
			}
			if (in.HasMember("hasMaterial") && in["hasMaterial"].IsBool() && !in["hasMaterial"].GetBool()) {
				materialGuid = {};
				m_material.reset();
				return;
			}

			// Restore params
			if (in.HasMember("params") && in["params"].IsObject()) {
				const auto& p = in["params"];

				auto readVec3 = [&](const char* name, const char* compatName = nullptr) {
					if (p.HasMember(name) && p[name].IsArray() && p[name].Size() == 3) {
						Vec3 v{ p[name][0].GetFloat(), p[name][1].GetFloat(), p[name][2].GetFloat() };
						m_material->SetVec3(name, v);
						// Also write compatibility alias if provided
						if (compatName) m_material->SetVec3(compatName, v);
					}
					};
				auto readFloat = [&](const char* name, const char* compatName = nullptr) {
					if (p.HasMember(name) && p[name].IsNumber()) {
						m_material->SetFloat(name, p[name].GetFloat());
						if (compatName) m_material->SetFloat(compatName, p[name].GetFloat());
					}
					};
				auto readInt = [&](const char* name) {
					if (p.HasMember(name) && p[name].IsInt())
						m_material->SetInt(name, p[name].GetInt());
					};
				auto readBool = [&](const char* name, const char* compatName = nullptr) {
					if (p.HasMember(name) && p[name].IsBool()) {
						m_material->SetBool(name, p[name].GetBool());
						if (compatName) m_material->SetBool(compatName, p[name].GetBool());
					}
					};

				// --- DESERIALIZE: accept vec4/vec3 + (alpha or transparency) and normalize to vec4 ---
				auto readVec4 = [&](const char* name, const char* compatName = nullptr) {
					if (p.HasMember(name) && p[name].IsArray() && p[name].Size() == 4) {
						Vec4 v{ p[name][0].GetFloat(), p[name][1].GetFloat(), p[name][2].GetFloat(), p[name][3].GetFloat() };
						m_material->SetVec4(name, v);
						if (compatName) m_material->SetVec4(compatName, v);
						// Keep explicit alpha/transparency mirrors in params for editor UIs
						m_material->SetFloat("materialAlpha", v.w);
						m_material->SetFloat("materialTransparency", 1.0f - v.w);
						return true;
					}
					return false;
					};

				auto readAlphaOrTransparency = [&]() -> std::optional<float> {
					// Prefer alpha if present; else compute from transparency
					if (p.HasMember("materialAlpha") && p["materialAlpha"].IsNumber())
						return p["materialAlpha"].GetFloat();
					if (p.HasMember("materialTransparency") && p["materialTransparency"].IsNumber()) {
						float tr = p["materialTransparency"].GetFloat();
						return 1.0f - tr;
					}
					return std::nullopt;
					};

				// Core PBR parameters (albedo first)
				bool readRGBA = readVec4("materialAlbedo", "material.albedo");
				if (!readRGBA) {
					// Legacy RGB load
					readVec3("materialAlbedo", "material.albedo");
					// If we only have RGB, try to augment with alpha/transparency
					if (auto alphaOpt = readAlphaOrTransparency()) {
						// Build a Vec4 from the currently set RGB (or defaults if absent)
						Vec3 rgb{ 0.8f, 0.8f, 0.8f };
						if (const auto* cur = m_material->GetParameter("materialAlbedo")) {
							if (cur->floatValues.size() >= 3) {
								rgb = Vec3(cur->floatValues[0], cur->floatValues[1], cur->floatValues[2]);
							}
						}
						const float a = std::clamp(*alphaOpt, 0.0f, 1.0f);
						m_material->SetVec4("materialAlbedo", Vec4(rgb.x, rgb.y, rgb.z, a));
						m_material->SetVec4("material.albedo", Vec4(rgb.x, rgb.y, rgb.z, a));
						m_material->SetFloat("materialAlpha", a);
						m_material->SetFloat("materialTransparency", 1.0f - a);
					}
				}
				else {
					// If RGBA was present, we've already mirrored alpha/transparency above
				}

				readFloat("materialMetallic", "material.metallic");
				readFloat("materialRoughness", "material.roughness");
				readFloat("materialAo", "material.ao");
				readVec3("materialEmissive", "material.emissive");
				readFloat("materialEmissiveIntensity", "material.emissiveIntensity");
				readFloat("materialNormalStrength", "material.normalStrength");
				readInt("materialShadingModel");
				readFloat("materialReflectance");
				readFloat("materialEnvironmentIntensity");

				// Map presence flags
				readBool("materialHasAlbedoMap");
				readBool("materialHasNormalMap", "material.hasNormalMap");
				readBool("materialHasRoughnessMap");
				readBool("materialHasMetallicMap");
				readBool("materialHasAoMap");
				readBool("materialHasEmissiveMap");
				readBool("materialHasEnvironmentMap");
				readBool("materialHasIrradianceMap");
			}
			// Restore textures
			if (in.HasMember("textures") && in["textures"].IsArray()) {
				const auto& arr = in["textures"];
				for (auto& t : arr.GetArray()) {
					if (!t.IsObject()) continue;
					if (!t.HasMember("slot") || !t.HasMember("path")) continue;
					if (!t["slot"].IsString() || !t["path"].IsString()) continue;

					std::string slot = t["slot"].GetString();
					std::string path = t["path"].GetString();

					auto tex = AssetManager::GetInstance().LoadTexture(path);
					if (tex && tex->IsValid()) {
						m_material->SetTexture(slot, tex);

						// For compatibility, mirror well-known slots to alternate names
						if (slot == "materialAlbedoMap")
							m_material->SetTexture("material.albedoMap", tex);
						else if (slot == "material.albedoMap")
							m_material->SetTexture("materialAlbedoMap", tex);

						// Update presence flags for known types
						auto setPresence = [&](const char* nonDot, const char* dot) {
							m_material->SetBool(nonDot, true);
							m_material->SetBool(dot, true);
							};
						if (slot.find("Normal") != std::string::npos) {
							setPresence("materialHasNormalMap", "material.hasNormalMap");
						}
						if (slot.find("Albedo") != std::string::npos || slot.find("albedo") != std::string::npos) {
							m_material->SetBool("materialHasAlbedoMap", true);
						}
						if (slot.find("Roughness") != std::string::npos || slot.find("roughness") != std::string::npos) {
							m_material->SetBool("materialHasRoughnessMap", true);
						}
						if (slot.find("Metallic") != std::string::npos || slot.find("metallic") != std::string::npos) {
							m_material->SetBool("materialHasMetallicMap", true);
						}
						if (slot.find("Ao") != std::string::npos || slot.find("ao") != std::string::npos || slot.find("AmbientOcclusion") != std::string::npos) {
							m_material->SetBool("materialHasAoMap", true);
						}
						if (slot.find("Emissive") != std::string::npos || slot.find("emissive") != std::string::npos) {
							m_material->SetBool("materialHasEmissiveMap", true);
						}
					}
				}
			}

			// Restore custom fragment shader path (if present)
			if (in.HasMember("customFragmentShader") && in["customFragmentShader"].IsString()) {
				customFragmentShader = in["customFragmentShader"].GetString();
			}

			// Restore shadow casting flag
			if (in.HasMember("castsShadows") && in["castsShadows"].IsBool()) {
				cacheCastsShadows = in["castsShadows"].GetBool();
				if (m_material) {
					m_material->SetBool("materialCastsShadows", cacheCastsShadows);
					// or whatever uniform name you use in the shader
				}
			}

			//  Ensure material has a valid shader after deserialization
			if (!m_material->GetShader() || !m_material->GetShader()->IsValid())
			{
				// If you have a custom fragment shader path, prefer that
				if (!customFragmentShader.empty()) {
					auto shader = AssetManager::GetInstance().LoadShader(
						"../Resources/Shaders/vertex.glsl",        // or your chosen vertex path
						customFragmentShader
					);

					if (shader && shader->IsValid()) {
						m_material->SetShader(shader);
						EE_CORE_INFO("Assigned custom fragment shader '{}' to material", customFragmentShader);
					}
					else {
						EE_CORE_WARN("Failed to load custom fragment shader '{}', falling back to default", customFragmentShader);
					}
				}

				// Fallback default if still invalid
				if (!m_material->GetShader() || !m_material->GetShader()->IsValid())
				{
					auto defaultShader = AssetManager::GetInstance().LoadShader(
						"../Resources/Shaders/vertex.glsl",
						"../Resources/Shaders/fragment_enhanced.glsl"
					);

					if (defaultShader && defaultShader->IsValid())
					{
						m_material->SetShader(defaultShader);
						EE_CORE_INFO("Auto-assigned default shader to material");
					}
					else
					{
						EE_CORE_WARN("Failed to assign default shader to material - shader loading failed");
					}
				}
			}

			if (auto gm = GetMaterial()) {
				// uvScale
				if (auto it = in.FindMember("uvScale"); it != in.MemberEnd() && it->value.IsArray() && it->value.Size() == 2) {
					gm->SetUVScale(JsonToVec2(it->value));
				}

				// uvOffset
				if (auto it = in.FindMember("uvOffset"); it != in.MemberEnd() && it->value.IsArray() && it->value.Size() == 2) {
					gm->SetUVOffset(JsonToVec2(it->value));
				}
			}

		}

		XPROPERTY_DEF(
			"Material", Material,
			// authoring template name
			xproperty::obj_member<"template", &Material::materialTemplate>,

			// custom shader and flags
			xproperty::obj_member<"fragmentShader", &Material::customFragmentShader>,
			xproperty::obj_member<"castsShadows", &Material::cacheCastsShadows>,

			// cached parameters
			xproperty::obj_member<"hasAlbedo", &Material::hasAlbedo>,
			xproperty::obj_member<"albedo", &Material::cacheAlbedo>,

			xproperty::obj_member<"hasRough", &Material::hasRough>,
			xproperty::obj_member<"roughness", &Material::cacheRoughness>,

			xproperty::obj_member<"hasMetal", &Material::hasMetal>,
			xproperty::obj_member<"metallic", &Material::cacheMetallic>,

			xproperty::obj_member<"hasEmiss", &Material::hasEmiss>,
			xproperty::obj_member<"emissive", &Material::cacheEmissive>,
			xproperty::obj_member<"emissiveIntensity", &Material::cacheEmissiveIntensity>
		)
	};

	struct GlobalGraphics
	{
		// Ambient lighting parameters
		Vec3  ambientColor = Vec3{ 1.0f, 1.0f, 1.0f };
		float ambientIntensity = 0.08f;

		// SSAO parameters
		bool  ssaoEnabled = false;
		int   ssaoSamples = 16;
		float ssaoRadius = 10.0f;
		float ssaoBias = 0.01f;
		float ssaoIntensity = 1.0f;
		float ssaoFadeout = 0.1f;
		float ssaoMaxDistance = 100.0f;

		// Fog parameters
		bool  fogEnabled = false;
		int   fogMode = 0;                     // 0 = linear, 1 = exp, 2 = exp^2
		Vec3  fogColor = Vec3{ 0.5f, 0.6f, 0.7f };
		float fogDensity = 0.02f;                 // exp modes
		float fogStart = 50.0f;                 // linear
		float fogEnd = 200.0f;                // linear
		float fogHeightCoefficient = 0.1f; // For height-based fog
		float fogHeightFalloff = 10.0f;      // For height-based fog

		// Post-processing toggles
		bool vignetteEnabled = false;
		bool fxaaEnabled = true;
		bool toneMappingEnabled = true;
		bool gammaCorrectionEnabled = true;
		bool bloomEnabled = true;
		bool skyboxIsHDR = false;
		bool showSkybox = true;

		// Post-processing parameters
		float exposure = 1.0f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float gamma = 2.2f;
		float vignetteIntensity = 0.3f;
		float vignetteRadius = 0.8f;
		float bloomStrength = 0.04f;

		// Film grain and chromatic aberration
		bool filmGrainEnabled = false;
		float grainIntensity = 0.015f;
		float grainScale = 1.5f;
		bool chromaticAberrationEnabled = false;
		float chromaticAmount = 0.003f;

		// FXAA parameters
		float fxaaSpanMax = 8.0f;
		float fxaaReduceMin = 1.0f / 128.0f;
		float fxaaReduceMul = 1.0f / 8.0f;

		// Bloom pass parameters
		float bloomThreshold = 1.0f;
		float bloomIntensity = 2.0f;
		float bloomRadius = 1.0f;

		// Spotlight ray parameters
		bool spotlightRaysEnabled = true;
		float spotlightRayIntensity = 0.3f;
		float spotlightRayFalloff = 2.0f;

		// === Motion blur parameters ===
		bool motionBlurEnabled = true;
		float motionBlurStrength = 1.0f;
		int motionBlurSamples = 8;

		// --- generic xproperty-based serialization ---
		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"GlobalGraphics", GlobalGraphics,

			// Ambient lighting
			xproperty::obj_member<"ambientColor", &GlobalGraphics::ambientColor>,
			xproperty::obj_member<"ambientIntensity", &GlobalGraphics::ambientIntensity>,

			// SSAO
			xproperty::obj_member<"ssaoEnabled", &GlobalGraphics::ssaoEnabled>,
			xproperty::obj_member<"ssaoSamples", &GlobalGraphics::ssaoSamples>,
			xproperty::obj_member<"ssaoRadius", &GlobalGraphics::ssaoRadius>,
			xproperty::obj_member<"ssaoBias", &GlobalGraphics::ssaoBias>,
			xproperty::obj_member<"ssaoIntensity", &GlobalGraphics::ssaoIntensity>,
			xproperty::obj_member<"ssaoFadeout", &GlobalGraphics::ssaoFadeout>,
			xproperty::obj_member<"ssaoMaxDistance", &GlobalGraphics::ssaoMaxDistance>,

			// Fog
			xproperty::obj_member<"fogEnabled", &GlobalGraphics::fogEnabled>,
			xproperty::obj_member<"fogMode", &GlobalGraphics::fogMode>,
			xproperty::obj_member<"fogColor", &GlobalGraphics::fogColor>,
			xproperty::obj_member<"fogDensity", &GlobalGraphics::fogDensity>,
			xproperty::obj_member<"fogStart", &GlobalGraphics::fogStart>,
			xproperty::obj_member<"fogEnd", &GlobalGraphics::fogEnd>,
			xproperty::obj_member<"fogHeightCoefficient", &GlobalGraphics::fogHeightCoefficient>,
			xproperty::obj_member<"fogHeightFalloff", &GlobalGraphics::fogHeightFalloff>,

			// Post-process toggles
			xproperty::obj_member<"vignetteEnabled", &GlobalGraphics::vignetteEnabled>,
			xproperty::obj_member<"fxaaEnabled", &GlobalGraphics::fxaaEnabled>,
			xproperty::obj_member<"toneMappingEnabled", &GlobalGraphics::toneMappingEnabled>,
			xproperty::obj_member<"gammaCorrectionEnabled", &GlobalGraphics::gammaCorrectionEnabled>,
			xproperty::obj_member<"bloomEnabled", &GlobalGraphics::bloomEnabled>,
			xproperty::obj_member<"skyboxIsHDR", &GlobalGraphics::skyboxIsHDR>,
			xproperty::obj_member<"showSkybox", &GlobalGraphics::showSkybox>,

			// Post-process params
			xproperty::obj_member<"exposure", &GlobalGraphics::exposure>,
			xproperty::obj_member<"contrast", &GlobalGraphics::contrast>,
			xproperty::obj_member<"saturation", &GlobalGraphics::saturation>,
			xproperty::obj_member<"gamma", &GlobalGraphics::gamma>,
			xproperty::obj_member<"vignetteIntensity", &GlobalGraphics::vignetteIntensity>,
			xproperty::obj_member<"vignetteRadius", &GlobalGraphics::vignetteRadius>,
			xproperty::obj_member<"bloomStrength", &GlobalGraphics::bloomStrength>,

			// Film grain and chromatic aberration
			xproperty::obj_member<"filmGrainEnabled", &GlobalGraphics::filmGrainEnabled>,
			xproperty::obj_member<"grainIntensity", &GlobalGraphics::grainIntensity>,
			xproperty::obj_member<"grainScale", &GlobalGraphics::grainScale>,
			xproperty::obj_member<"chromaticAberrationEnabled", &GlobalGraphics::chromaticAberrationEnabled>,
			xproperty::obj_member<"chromaticAmount", &GlobalGraphics::chromaticAmount>,

			// FXAA
			xproperty::obj_member<"fxaaSpanMax", &GlobalGraphics::fxaaSpanMax>,
			xproperty::obj_member<"fxaaReduceMin", &GlobalGraphics::fxaaReduceMin>,
			xproperty::obj_member<"fxaaReduceMul", &GlobalGraphics::fxaaReduceMul>,

			// Bloom pass
			xproperty::obj_member<"bloomThreshold", &GlobalGraphics::bloomThreshold>,
			xproperty::obj_member<"bloomIntensity", &GlobalGraphics::bloomIntensity>,
			xproperty::obj_member<"bloomRadius", &GlobalGraphics::bloomRadius>,

			// Spotlight ray parameters
			xproperty::obj_member<"spotlightRaysEnabled", &GlobalGraphics::spotlightRaysEnabled>,
			xproperty::obj_member<"spotlightRayIntensity", &GlobalGraphics::spotlightRayIntensity>,
			xproperty::obj_member<"spotlightRayFalloff", &GlobalGraphics::spotlightRayFalloff>,
			
			// Motion blur
			xproperty::obj_member<"motionBlurEnabled", &GlobalGraphics::motionBlurEnabled>,
			xproperty::obj_member<"motionBlurStrength", &GlobalGraphics::motionBlurStrength>,
			xproperty::obj_member<"motionBlurSamples", &GlobalGraphics::motionBlurSamples>
		)
	};

	/*!***********************************************************************
	\brief
	 Light structure
	*************************************************************************/
	struct Light {
		Vec3 color{};
		float intensity{};
		LightType type{};
		bool castsShadows{ false };
		bool castsRays{ false }; // For volumetric light shafts/god rays
		glm::mat4 lightSpaceMatrices[NUM_CASCADES]{}; // For shadow mapping
		glm::mat4 pointLightMatrices[6]{}; // Cubemap faces for point light shadows
		int startOffset{ 0 }; // For UBO indexing
		float innerAngle{ -1.0f }; // For spotlights
		float outerAngle{ -1.0f }; // For spotlights
		float radius{ 3.0f }; // For point lights/spotlights
		float splitDepths[NUM_CASCADES]{};

		Light() : color(1.0f, 1.0f, 1.0f),
			intensity(1.0f),
			type(LightType::POINT),
			lightSpaceMatrices{},
			splitDepths{}
		{
		}

		Light(const Vec3& col, float intens, LightType t) :
			color(col), intensity(intens), type(t),
			lightSpaceMatrices{},
			splitDepths{}
		{
		}

		Light(const Vec3& col, float intens, LightType t, bool shadows) :
			color(col), intensity(intens), type(t), castsShadows(shadows),
			lightSpaceMatrices{},
			splitDepths{}
		{
		}

		Light(const Vec3& col, float intens, LightType t, bool shadows, float inner, float outer, float rad = 1.0f) :
			color(col), intensity(intens), type(t), castsShadows(shadows), innerAngle(inner), outerAngle(outer), radius(rad),
			lightSpaceMatrices{},
			splitDepths{}
		{
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"Light", Light,
			xproperty::obj_member<"color", &Light::color>,
			xproperty::obj_member<"intensity", &Light::intensity>,
			xproperty::obj_member<"type", &Light::type>,
			xproperty::obj_member<"castsShadows", &Light::castsShadows>,
			xproperty::obj_member<"castsRays", &Light::castsRays>,
			xproperty::obj_member<"innerAngle", &Light::innerAngle>,  // used for spot
			xproperty::obj_member<"outerAngle", &Light::outerAngle>,  // used for spot
			xproperty::obj_member<"radius", &Light::radius>       // used for point/spot
		)
	};

	/*!***********************************************************************
		AudioSource structure for individual audio files.
	*************************************************************************/
	struct AudioSource
	{
		std::string audioPath{};
		std::string audioName{};
		float volume{ 0.2f }; // Volume from 0.0f to 1.0f (matches your previous engine)

		AudioSource() = default;
		AudioSource(const std::string& name, const std::string& path, float vol = 0.2f) :
			audioName(name), audioPath(path), volume(vol) {
		}

		// Reflect AudioSource (name, path, volume)
		XPROPERTY_DEF(
			"AudioSource", AudioSource,
			xproperty::obj_member<"name", &AudioSource::audioName>,
			xproperty::obj_member<"path", &AudioSource::audioPath>,
			xproperty::obj_member<"volume", &AudioSource::volume>
		)
	};

	/*!***********************************************************************
	\brief
	 Global AudioManager component for managing music, SFX collections, and global audio.
	 Similar to your previous engine but adapted for FMOD.
	*************************************************************************/
	struct GlobalAudioComponent
	{
		std::vector<AudioSource> music; // Music category
		std::vector<AudioSource> sfx;   // SFX category
		std::vector<AudioSource> ambience;

		// Global volume controls
		float masterVolume{ 1.0f };
		float musicVolume{ 1.0f };
		float sfxVolume{ 1.0f };
		float ambienceVolume{ 1.0f };  // *** NEW ***

		bool autoPlay{true};

		// Currently playing tracks
		int currentMusicIndex{ -1 };
		int currentMusicChannelId{ -1 };

		// *** NEW: Ambience tracking ***
		int currentAmbienceIndex{ -1 };
		int currentAmbienceChannelId{ -1 };

		GlobalAudioComponent() = default;

		// Music management
		void PlayMusic(int index);
		void StopMusic();
		void SetMusicVolume(float volume);

		// *** NEW: Ambience management ***
		void PlayAmbience(int index);
		void PlayAmbience(const std::string& name);
		void StopAmbience();
		void SetAmbienceVolume(float volume);
		int GetAmbienceIndex(const std::string& name) const;
		void AddAmbienceSource(const std::string& name, const std::string& path);
		void UpdateAmbienceSource(int index, const std::string& name, const std::string& path);
		void RemoveAmbienceSource(int index);
		const AudioSource* GetAmbienceSource(int index) const;
		int FindAmbienceIndex(const std::string& name) const;

		// SFX management
		void PlaySFX(int index);
		void PlaySFX(const std::string& name);
		void SetSFXVolume(float volume);

		// Utility functions
		int GetSFXIndex(const std::string& name) const;
		int GetMusicIndex(const std::string& name) const;
		void AddMusicSource(const std::string& name, const std::string& path);
		void AddSFXSource(const std::string& name, const std::string& path);

		void UpdateMusicSource(int index, const std::string& name, const std::string& path)
		{
			if (index >= 0 && index < static_cast<int>(music.size()))
			{
				// Stop current music if we're updating the currently playing track
				if (currentMusicIndex == index && currentMusicChannelId != -1)
				{
					CAudioEngine::StopChannel(currentMusicChannelId);
					currentMusicChannelId = -1;
				}

				// Update the music source
				music[index].audioName = name;
				music[index].audioPath = path;

				// Reload the sound with new path
				try
				{
					CAudioEngine::LoadSound(path, false, true, true); // Background music is typically looped and streamed
				}
				catch (const std::exception& e)
				{
					(void)e;
					// Handle loading error if needed
					UNREFERENCED_PARAMETER(e);
				}
			}
		}

		// Update an existing SFX source
		void UpdateSFXSource(int index, const std::string& name, const std::string& path)
		{
			if (index >= 0 && index < static_cast<int>(sfx.size()))
			{
				// Update the SFX source
				sfx[index].audioName = name;
				sfx[index].audioPath = path;

				// Reload the sound with new path
				try
				{
					CAudioEngine::LoadSound(path, false, false, false); // SFX typically not looped/streamed
				}
				catch (const std::exception& e)
				{
					(void)e;
					// Handle loading error if needed
					UNREFERENCED_PARAMETER(e);
				}
			}
		}

		// Remove a music source
		void RemoveMusicSource(int index)
		{
			if (index >= 0 && index < static_cast<int>(music.size()))
			{
				// Stop current music if we're removing the currently playing track
				if (currentMusicIndex == index && currentMusicChannelId != -1)
				{
					CAudioEngine::StopChannel(currentMusicChannelId);
					currentMusicChannelId = -1;
					currentMusicIndex = -1;
				}
				else if (currentMusicIndex > index)
				{
					// Adjust current music index if a track before it was removed
					currentMusicIndex--;
				}

				// Remove the music source
				music.erase(music.begin() + index);
			}
		}

		// Remove an SFX source
		void RemoveSFXSource(int index)
		{
			if (index >= 0 && index < static_cast<int>(sfx.size()))
			{
				// Remove the SFX source
				sfx.erase(sfx.begin() + index);
			}
		}

		// Get music source by index (for safe access)
		const AudioSource* GetMusicSource(int index) const
		{
			if (index >= 0 && index < static_cast<int>(music.size()))
			{
				return &music[index];
			}
			return nullptr;
		}

		// Get SFX source by index (for safe access)
		const AudioSource* GetSFXSource(int index) const
		{
			if (index >= 0 && index < static_cast<int>(sfx.size()))
			{
				return &sfx[index];
			}
			return nullptr;
		}

		// Find music index by name
		int FindMusicIndex(const std::string& name) const
		{
			for (size_t i = 0; i < music.size(); ++i)
			{
				if (music[i].audioName == name)
				{
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		// Find SFX index by name
		int FindSFXIndex(const std::string& name) const
		{
			for (size_t i = 0; i < sfx.size(); ++i)
			{
				if (sfx[i].audioName == name)
				{
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		// GlobalAudioComponent
		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("masterVolume", masterVolume, alloc);
			out.AddMember("musicVolume", musicVolume, alloc);
			out.AddMember("sfxVolume", sfxVolume, alloc);
			out.AddMember("ambienceVolume", ambienceVolume, alloc);
			out.AddMember("autoPlay", autoPlay, alloc);

			auto writeList = [&](const std::vector<AudioSource>& list, const char* key) {
				rapidjson::Value arr(rapidjson::kArrayType);
				for (auto& s : list) {
					rapidjson::Value o(rapidjson::kObjectType);
					o.AddMember("name", rapidjson::Value(s.audioName.c_str(), alloc), alloc);
					o.AddMember("path", rapidjson::Value(s.audioPath.c_str(), alloc), alloc);
					o.AddMember("volume", s.volume, alloc);
					arr.PushBack(o, alloc);
				}
				out.AddMember(rapidjson::StringRef(key), arr, alloc);
				};
			writeList(music, "music");
			writeList(sfx, "sfx");
			writeList(ambience, "ambience");
		}
		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("masterVolume")) masterVolume = in["masterVolume"].GetFloat();
			if (in.HasMember("musicVolume")) musicVolume = in["musicVolume"].GetFloat();
			if (in.HasMember("sfxVolume"))   sfxVolume = in["sfxVolume"].GetFloat();
			if (in.HasMember("ambienceVolume")) ambienceVolume = in["ambienceVolume"].GetFloat();
			if (in.HasMember("autoPlay"))    autoPlay = in["autoPlay"].GetBool();

			auto readList = [&](const char* key, std::vector<AudioSource>& list) {
				list.clear();
				if (in.HasMember(key) && in[key].IsArray()) {
					for (auto& v : in[key].GetArray()) {
						AudioSource s;
						if (v.HasMember("name") && v["name"].IsString())   s.audioName = v["name"].GetString();
						if (v.HasMember("path") && v["path"].IsString())   s.audioPath = v["path"].GetString();
						if (v.HasMember("volume") && v["volume"].IsNumber()) s.volume = v["volume"].GetFloat();
						list.push_back(std::move(s));
					}
				}
				};
			readList("music", music);
			readList("sfx", sfx);
			readList("ambience", ambience);

			currentMusicIndex = -1; currentMusicChannelId = -1;
			currentAmbienceIndex = -1;      // *** NEW ***
			currentAmbienceChannelId = -1;  // *** NEW ***
		}

		XPROPERTY_DEF(
			"GlobalAudioComponent", GlobalAudioComponent,
			xproperty::obj_member<"masterVolume", &GlobalAudioComponent::masterVolume>,
			xproperty::obj_member<"musicVolume", &GlobalAudioComponent::musicVolume>,
			xproperty::obj_member<"sfxVolume", &GlobalAudioComponent::sfxVolume>,
			xproperty::obj_member<"ambienceVolume", &GlobalAudioComponent::ambienceVolume>,
			xproperty::obj_member<"autoPlay", &GlobalAudioComponent::autoPlay>
		)
	};

	/*!***********************************************************************
	\brief
	 Individual AudioComponent for entity-specific audio (footsteps, weapon sounds, etc.)
	 Works alongside the global AudioManager.
	*************************************************************************/
	struct AudioComponent
	{
		// Basic audio properties
		std::string soundName{};
		std::string eventName{}; // For FMOD Studio events

		std::vector<std::string> soundVariations{};  // Audio bank for random variations
		bool useRandomVariation{ false };            // Toggle between single sound and variations
		int lastPlayedVariationIndex{ -1 };

		// Playback control
		int channelId{ -1 }; // Managed by CAudioEngine
		bool isPlaying{ false };
		bool shouldPlay{ false }; // Trigger flag for AudioSystem
		bool shouldStop{ false }; // Trigger flag for AudioSystem
		bool playOnStart{ false };

		// Audio settings
		bool is3D{ true };
		bool isLooping{ false };
		bool isStreaming{ false };
		float volume{ 0.5f }; // Volume from 0.0f to 1.0f (NOT dB!) - will be converted to dB when needed

		// 3D Audio properties
		bool followTransform{ true }; // Should audio follow entity position?
		float minDistance{ 1.0f }; // 3D audio rolloff settings
		float maxDistance{ 100.0f };

		// FMOD Studio event parameters (optional)
		std::map<std::string, float> eventParameters{};

		// Constructors
		AudioComponent() = default;
		explicit AudioComponent(const std::string& sound, bool is3d = true, bool loop = false, float vol = 0.5f) :
			soundName(sound), is3D(is3d), isLooping(loop), volume(vol) {
		}
		explicit AudioComponent(const std::string& event) :
			eventName(event), is3D(false), volume(0.5f) {
		} // Events typically handle their own 3D settings

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			// Use xproperty for most fields
			xprop_utils::SerializeToJson(*this, out, alloc);

			// Manually serialize soundVariations as a proper array
			if (!soundVariations.empty()) {
				rapidjson::Value arr(rapidjson::kArrayType);
				for (const auto& variation : soundVariations) {
					arr.PushBack(rapidjson::Value(variation.c_str(), alloc), alloc);
				}
				out.AddMember("soundVariations", arr, alloc);
			}
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);

			// Manually deserialize soundVariations array
			soundVariations.clear();
			if (in.HasMember("soundVariations") && in["soundVariations"].IsArray()) {
				for (const auto& v : in["soundVariations"].GetArray()) {
					if (v.IsString()) {
						soundVariations.push_back(v.GetString());
					}
				}
			}

			lastPlayedVariationIndex = -1;
		}

		XPROPERTY_DEF(
			"AudioComponent", AudioComponent,
			xproperty::obj_member<"soundName", &AudioComponent::soundName>,
			xproperty::obj_member<"eventName", &AudioComponent::eventName>,   
			xproperty::obj_member<"useRandomVariation", &AudioComponent::useRandomVariation>,
			xproperty::obj_member<"is3D", &AudioComponent::is3D>,
			xproperty::obj_member<"isLooping", &AudioComponent::isLooping>,
			xproperty::obj_member<"isStreaming", &AudioComponent::isStreaming>,
			xproperty::obj_member<"volume", &AudioComponent::volume>,
			xproperty::obj_member<"followTransform", &AudioComponent::followTransform>,
			xproperty::obj_member<"minDistance", &AudioComponent::minDistance>,
			xproperty::obj_member<"maxDistance", &AudioComponent::maxDistance>,
			xproperty::obj_member<"playOnStart", &AudioComponent::playOnStart>
		)
	};

	/*!***********************************************************************
	 \brief
	 Particle component structure.
	*************************************************************************/
	/*struct Particle
	{
		Vec3 velocity;
		float lifetime;
		float age;
		Vec4 colour;
		float size;

		Particle() : velocity(0, 0, 0), lifetime(1.0f), age(0.0f), colour(1, 1, 1, 1), size(1.0f) {}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("velocity", Vec3ToJson(velocity, alloc), alloc);
			out.AddMember("lifetime", lifetime, alloc);
			out.AddMember("age", age, alloc);
			out.AddMember("colour", Vec4ToJson(colour, alloc), alloc);
			out.AddMember("size", size, alloc);
		}
		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("velocity")) velocity = JsonToVec3(in["velocity"]);
			if (in.HasMember("lifetime")) lifetime = in["lifetime"].GetFloat();
			if (in.HasMember("age"))      age = in["age"].GetFloat();
			if (in.HasMember("colour") && in["colour"].IsArray() && in["colour"].Size() == 4)
				colour = Vec4(in["colour"][0].GetFloat(), in["colour"][1].GetFloat(), in["colour"][2].GetFloat(), in["colour"][3].GetFloat());
			if (in.HasMember("size"))     size = in["size"].GetFloat();
		}
	};*/

	struct ParticleEmitter
	{
		bool active = true;
		float emissionRate = 10.0f;  // particles per second
		float particleLifetime = 2.0f;
		float particleSize = 0.2f;
		Vec3 velocity = { 0, 2, 0 };
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		std::string textureName = "../Resources/Textures/greybox_light_solid.png";
		float timeAccumulator = 0.0f;

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"ParticleEmitterComponent", ParticleEmitter,
			xproperty::obj_member<"active", &ParticleEmitter::active>,
			xproperty::obj_member<"emissionRate", &ParticleEmitter::emissionRate>,
			xproperty::obj_member<"particleLifetime", &ParticleEmitter::particleLifetime>,
			xproperty::obj_member<"particleSize", &ParticleEmitter::particleSize>,
			xproperty::obj_member<"velocity", &ParticleEmitter::velocity>,
			xproperty::obj_member<"textureName", &ParticleEmitter::textureName>
		);
	};

	/*!***********************************************************************
	 \brief
	  GPU-based particle emitter (standard emitter model).
	*************************************************************************/
	struct GPUParticleEmitter
	{
		bool active = true;
		int maxParticles = 256;

		// Emission
		int emissionShape = 0; // 0 = point, 1 = sphere, 2 = box, 3 = disc (XZ)
		Vec3 localPositionOffset = Vec3(0.0f, 0.0f, 0.0f);  // Offset from parent transform
		float overallScale = 1.0f;  // Master scale for all size/radius properties
		Vec3 spawnBoxExtents = Vec3(0.5f, 0.5f, 0.5f);
		float spawnRadius = 0.5f;
		float spawnRadiusInner = 0.0f;
		float spawnRate = 20.0f; // particles/sec
		int burstCountMin = 0;
		int burstCountMax = 0;
		float burstInterval = 0.0f;
		bool burstOnStart = false;

		// Direction
		int directionMode = 0; // 0 = fixed, 1 = cone, 2 = from spawn, 3 = random sphere
		Vec3 direction = Vec3(0.0f, 0.0f, 1.0f);
		float coneAngle = 25.0f;
		float coneInnerAngle = 0.0f;

		// Forces & speed
		float speedMin = 0.5f;
		float speedMax = 3.0f;
		Vec3 gravity = Vec3(0.0f, -2.0f, 0.0f);
		float drag = 0.0f;
		float turbulenceStrength = 0.0f;
		float turbulenceScale = 1.0f;

		// Bounds
		int boundsMode = 0; // 0 = none, 1 = kill, 2 = clamp, 3 = bounce
		int boundsShape = 0; // 0 = sphere, 1 = box, 2 = disc (XZ)
		Vec3 boundsBoxExtents = Vec3(1.0f, 1.0f, 1.0f);
		float boundsRadius = 2.0f;
		float boundsRadiusInner = 0.0f;

		// Appearance
		int renderMode = 0; // 0 = glow, 1 = smoke, 2 = electric
		float smokeOpacity = 0.6f;
		float smokeSoftness = 0.5f;
		float smokeNoiseScale = 0.15f;
		float smokeDistortScale = 0.25f;
		float smokeDistortStrength = 0.35f;
		float smokePuffScale = 0.35f;
		float smokePuffStrength = 0.6f;
		float smokeStretch = 0.5f;
		float smokeUpBias = 0.2f;
		float smokeDepthFade = 6.0f;
		float electricIntensity = 1.0f;
		float electricFrequency = 8.0f;
		float electricBoltThickness = 0.08f;
		float electricBoltVariation = 1.5f;
		float electricGlow = 0.5f;
		int electricBoltCount = 3;
		Vec3 colorStart = Vec3(1.0f, 0.85f, 0.2f);
		Vec3 colorEnd = Vec3(0.1f, 0.1f, 0.1f);
		float alphaStart = 1.0f;
		float alphaEnd = 0.0f;
		float sizeStartMin = 0.03f;
		float sizeStartMax = 0.08f;
		float sizeEndMin = 0.02f;
		float sizeEndMax = 0.06f;
		float lifetimeMin = 0.6f;
		float lifetimeMax = 1.8f;
		int sparkleShape = 0;  // 0 = soft circle, 1 = star, 2 = diamond

		// Runtime state (not serialized)
		float spawnAccumulator = 0.0f;
		float burstTimer = 0.0f;
		bool burstPrimed = false;
		unsigned int particleBuffer = 0;
		unsigned int spawnCounterBuffer = 0;
		bool initialized = false;
		bool showDebugBounds = false;  // Debug visualization toggle

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"GPUParticleEmitterComponent", GPUParticleEmitter,
			xproperty::obj_member<"active", &GPUParticleEmitter::active>,
			xproperty::obj_member<"maxParticles", &GPUParticleEmitter::maxParticles>,
			xproperty::obj_member<"emissionShape", &GPUParticleEmitter::emissionShape>,
			xproperty::obj_member<"localPositionOffset", &GPUParticleEmitter::localPositionOffset>,
			xproperty::obj_member<"overallScale", &GPUParticleEmitter::overallScale>,
			xproperty::obj_member<"spawnBoxExtents", &GPUParticleEmitter::spawnBoxExtents>,
			xproperty::obj_member<"spawnRadius", &GPUParticleEmitter::spawnRadius>,
			xproperty::obj_member<"spawnRadiusInner", &GPUParticleEmitter::spawnRadiusInner>,
			xproperty::obj_member<"spawnRate", &GPUParticleEmitter::spawnRate>,
			xproperty::obj_member<"burstCountMin", &GPUParticleEmitter::burstCountMin>,
			xproperty::obj_member<"burstCountMax", &GPUParticleEmitter::burstCountMax>,
			xproperty::obj_member<"burstInterval", &GPUParticleEmitter::burstInterval>,
			xproperty::obj_member<"burstOnStart", &GPUParticleEmitter::burstOnStart>,
			xproperty::obj_member<"directionMode", &GPUParticleEmitter::directionMode>,
			xproperty::obj_member<"direction", &GPUParticleEmitter::direction>,
			xproperty::obj_member<"coneAngle", &GPUParticleEmitter::coneAngle>,
			xproperty::obj_member<"coneInnerAngle", &GPUParticleEmitter::coneInnerAngle>,
			xproperty::obj_member<"speedMin", &GPUParticleEmitter::speedMin>,
			xproperty::obj_member<"speedMax", &GPUParticleEmitter::speedMax>,
			xproperty::obj_member<"gravity", &GPUParticleEmitter::gravity>,
			xproperty::obj_member<"drag", &GPUParticleEmitter::drag>,
			xproperty::obj_member<"turbulenceStrength", &GPUParticleEmitter::turbulenceStrength>,
			xproperty::obj_member<"turbulenceScale", &GPUParticleEmitter::turbulenceScale>,
			xproperty::obj_member<"boundsMode", &GPUParticleEmitter::boundsMode>,
			xproperty::obj_member<"boundsShape", &GPUParticleEmitter::boundsShape>,
			xproperty::obj_member<"boundsBoxExtents", &GPUParticleEmitter::boundsBoxExtents>,
			xproperty::obj_member<"boundsRadius", &GPUParticleEmitter::boundsRadius>,
			xproperty::obj_member<"boundsRadiusInner", &GPUParticleEmitter::boundsRadiusInner>,
			xproperty::obj_member<"renderMode", &GPUParticleEmitter::renderMode>,
			xproperty::obj_member<"smokeOpacity", &GPUParticleEmitter::smokeOpacity>,
			xproperty::obj_member<"smokeSoftness", &GPUParticleEmitter::smokeSoftness>,
			xproperty::obj_member<"smokeNoiseScale", &GPUParticleEmitter::smokeNoiseScale>,
			xproperty::obj_member<"smokeDistortScale", &GPUParticleEmitter::smokeDistortScale>,
			xproperty::obj_member<"smokeDistortStrength", &GPUParticleEmitter::smokeDistortStrength>,
			xproperty::obj_member<"smokePuffScale", &GPUParticleEmitter::smokePuffScale>,
			xproperty::obj_member<"smokePuffStrength", &GPUParticleEmitter::smokePuffStrength>,
			xproperty::obj_member<"smokeStretch", &GPUParticleEmitter::smokeStretch>,
			xproperty::obj_member<"smokeUpBias", &GPUParticleEmitter::smokeUpBias>,
			xproperty::obj_member<"smokeDepthFade", &GPUParticleEmitter::smokeDepthFade>,
			xproperty::obj_member<"electricIntensity", &GPUParticleEmitter::electricIntensity>,
			xproperty::obj_member<"electricFrequency", &GPUParticleEmitter::electricFrequency>,
			xproperty::obj_member<"electricBoltThickness", &GPUParticleEmitter::electricBoltThickness>,
			xproperty::obj_member<"electricBoltVariation", &GPUParticleEmitter::electricBoltVariation>,
			xproperty::obj_member<"electricGlow", &GPUParticleEmitter::electricGlow>,
			xproperty::obj_member<"electricBoltCount", &GPUParticleEmitter::electricBoltCount>,
			xproperty::obj_member<"colorStart", &GPUParticleEmitter::colorStart>,
			xproperty::obj_member<"colorEnd", &GPUParticleEmitter::colorEnd>,
			xproperty::obj_member<"alphaStart", &GPUParticleEmitter::alphaStart>,
			xproperty::obj_member<"alphaEnd", &GPUParticleEmitter::alphaEnd>,
			xproperty::obj_member<"sizeStartMin", &GPUParticleEmitter::sizeStartMin>,
			xproperty::obj_member<"sizeStartMax", &GPUParticleEmitter::sizeStartMax>,
			xproperty::obj_member<"sizeEndMin", &GPUParticleEmitter::sizeEndMin>,
			xproperty::obj_member<"sizeEndMax", &GPUParticleEmitter::sizeEndMax>,
			xproperty::obj_member<"lifetimeMin", &GPUParticleEmitter::lifetimeMin>,
			xproperty::obj_member<"lifetimeMax", &GPUParticleEmitter::lifetimeMax>,
			xproperty::obj_member<"sparkleShape", &GPUParticleEmitter::sparkleShape>
		);
	};

	/*!***********************************************************************
	 \brief
	  Hierarchy component structure for parent-child relationships.
	*************************************************************************/
	struct HierarchyComponent
	{
		static constexpr EntityID INVALID_PARENT = 0;

		EntityID parent = INVALID_PARENT;        // Parent entity ID
		std::vector<EntityID> children;         // List of child entity IDs
		int depth = 0;                          // Depth in hierarchy (root = 0)
		bool isDirty = false;                   // Flag for transform updates

		// Optional: Cache world transform for performance
		Mtx44 worldTransform{ 1.0f };             // Cached world transform
		bool worldTransformDirty = true;        // Separate flag for world transform cache

		// --- serialized form ---
		Guid parentGuid;                             // guid of my parent (Nil if root)
		std::vector<Guid> childrenGuids;            // guids of my direct children

		// Constructors
		HierarchyComponent() = default;

		explicit HierarchyComponent(EntityID parentId)
			: parent(parentId), depth(0), isDirty(true), worldTransformDirty(true)
		{
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();

			// Write parentGuid from runtime parent
			if (parent != INVALID_PARENT) {
				auto& ecs = ECS::GetInstance();
				if (ecs.HasComponent<IDComponent>(parent)) {
					const auto& pid = ecs.GetComponent<IDComponent>(parent);
					const std::string g = pid.guid.ToString();
					rapidjson::Value s;
					s.SetString(g.c_str(), (rapidjson::SizeType)g.size(), alloc);
					out.AddMember("parentGuid", s, alloc);
				}
			}
			// optional editor fields
			rapidjson::Value dv; dv.SetInt(depth);
			out.AddMember("depth", dv, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (!in.IsObject()) return;

			// Reset runtime links; we rebuild them later in a resolve pass
			parent = INVALID_PARENT;
			children.clear();

			// --- Read GUID-based form (authoritative on disk) ---
			parentGuid = {};
			childrenGuids.clear();

			if (in.HasMember("parentGuid") && in["parentGuid"].IsString()) {
				parentGuid = Guid::FromString(in["parentGuid"].GetString());
			}

			if (in.HasMember("childrenGuids") && in["childrenGuids"].IsArray()) {
				for (const auto& v : in["childrenGuids"].GetArray()) {
					if (v.IsString()) {
						childrenGuids.push_back(Guid::FromString(v.GetString()));
					}
				}
			}

			// --- Legacy numeric fallback ONLY if no GUID present ---
			if (!parentGuid.IsValid()) {
				if (in.HasMember("parent") && in["parent"].IsUint64())
					parent = static_cast<EntityID>(in["parent"].GetUint64());

				if (in.HasMember("children") && in["children"].IsArray()) {
					const auto& arr = in["children"].GetArray();
					children.reserve(arr.Size());
					for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
						if (arr[i].IsUint64())
							children.push_back(static_cast<EntityID>(arr[i].GetUint64()));
					}
				}
			}

			// Depth (editor/UI)
			depth = (in.HasMember("depth") && in["depth"].IsInt())
				? in["depth"].GetInt() : 0;

			// Housekeeping
			isDirty = true;
			worldTransform = Mtx44{ 1.0f };
			worldTransformDirty = true;
		}

		XPROPERTY_DEF(
			"HierarchyComponent", HierarchyComponent,
			xproperty::obj_member<"depth", &HierarchyComponent::depth>
		);
	};
}

// Forward declarations for NavMesh
struct dtNavMesh;
struct dtNavMeshQuery;
namespace Ermine
{
	/*!***********************************************************************
	 \brief
	 Physic component structure.
	*************************************************************************/
	struct PhysicComponent
	{
		//collision type
		PhysicsBodyType bodyType{ PhysicsBodyType::Rigid };
		//obj type
		JPH::EMotionType motionType{ JPH::EMotionType::Static };
		//obj weight
		float mass{ 0.0f };
		//collision shape
		ShapeType shapeType{ ShapeType::Box };
		//collision transform & constrains
		Ermine::Vec3 colliderPivot{ 0,0,0 };
		bool posX = false; bool posY = false; bool posZ = false;
		Ermine::Vec3 colliderRot{ 0,0,0 };
		bool rotX = false; bool rotY = false; bool rotZ = false;
		Ermine::Vec3 colliderSize{ 1,1,1 };

		Vec3 prevTranPos{};
		Quaternion prevTranRot{};
		Vec3 prevTranScale{};
		bool update = false;

		JPH::BodyID bodyID{ JPH::BodyID::cInvalidBodyID };
		JPH::Body* body{ nullptr };
		std::vector<glm::vec3> customMeshVertices;   // For custom mesh
		JPH::RefConst<JPH::Shape> shapeRef;
		bool isDead = false;

		PhysicComponent() = default;

		PhysicComponent(
			PhysicsBodyType type,
			JPH::EMotionType motion,
			float m = 0.0f,
			ShapeType shape = ShapeType::Box)
			: bodyType(type), motionType(motion), mass(m), shapeType(shape)
		{
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			xprop_utils::SerializeToJson(*this, out, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			xprop_utils::DeserializeFromJson(*this, in);
		}

		XPROPERTY_DEF(
			"PhysicComponent", PhysicComponent,
			xproperty::obj_member<"bodyType", &PhysicComponent::bodyType>,
			xproperty::obj_member<"motionType", &PhysicComponent::motionType>,
			xproperty::obj_member<"mass", &PhysicComponent::mass>,
			xproperty::obj_member<"shapeType", &PhysicComponent::shapeType>,
			xproperty::obj_member<"colliderpivot", &PhysicComponent::colliderPivot>,
			xproperty::obj_member<"posx", &PhysicComponent::posX>,
			xproperty::obj_member<"posy", &PhysicComponent::posY>,
			xproperty::obj_member<"posz", &PhysicComponent::posZ>,
			xproperty::obj_member<"colliderrot", &PhysicComponent::colliderRot>,
			xproperty::obj_member<"rotx", &PhysicComponent::rotX>,
			xproperty::obj_member<"roty", &PhysicComponent::rotY>,
			xproperty::obj_member<"rotz", &PhysicComponent::rotZ>,
			xproperty::obj_member<"collidersize", &PhysicComponent::colliderSize>
		)
	};

	/*!***********************************************************************
	\brief
	 Model component structure.
	*************************************************************************/
	struct ModelComponent
	{
		std::shared_ptr<graphics::Model> m_model;
		//std::string m_modelPath;  // Store the path for serialization
		bool m_isSkinFile = false; // Track if it's a .skin file

		ModelComponent() = default;

		explicit ModelComponent(const std::shared_ptr<graphics::Model>& model)
			: m_model(model) {
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();

			// Serialize model path
			rapidjson::Value pathVal;
			pathVal.SetString(m_model->GetName().c_str(),
				static_cast<rapidjson::SizeType>(m_model->GetName().size()),
				alloc);
			out.AddMember("model", pathVal, alloc);

			// Serialize file type flag
			out.AddMember("isSkinFile", m_isSkinFile, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (in.HasMember("model") && in["model"].IsString()) {
				const char* name = in["model"].GetString();
				std::string modelPath = "../Resources/Models/" + std::string(name);

				// Check if it's a .skin file
				if (in.HasMember("isSkinFile") && in["isSkinFile"].IsBool()) {
					m_isSkinFile = in["isSkinFile"].GetBool();
				}
				else {
					// Auto-detect based on file extension
					std::string ext = std::filesystem::path(modelPath).extension().string();
					m_isSkinFile = (ext == ".skin");
				}
				// Try to get cached model first
				m_model = AssetManager::GetInstance().GetModel(modelPath);

				// If not cached, load it (which will also cache it)
				if (!m_model) {
					m_model = AssetManager::GetInstance().LoadModel(modelPath);
				}
			}
		}

		//XPROPERTY_DEF(
		//	"ModelComponent", ModelComponent,
		//	xproperty::obj_member<"modelName", &ModelComponent::modelName>
		//)
	};

	/*!***********************************************************************
	\brief
	 Animation component structure.

	 Each entity with this component has its own Animator instance, allowing
	 multiple instances of the same model to have independent animation states.
	 Each entity gets its own bone transform offset in the SkeletalSSBO.
	*************************************************************************/
	struct AnimationComponent
	{
		std::shared_ptr<graphics::Animator> m_animator;         // Per-entity animator (independent state)
		int boneTransformOffset = -1;                           // Per-entity bone offset in SkeletalSSBO (allocated by AnimationManager)
		std::shared_ptr<AnimationGraph> m_animationGraph;		// Handles animation states and transitions

		bool initialized = false;								// Flag to initialize animation graph on first update

		AnimationComponent() : m_animationGraph(std::make_shared<AnimationGraph>()) {}
		explicit AnimationComponent(const std::shared_ptr<graphics::Model>& model)
			: m_animator(std::make_shared<graphics::Animator>(model)), m_animationGraph(std::make_shared<AnimationGraph>()) {
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();

			// -----------------
			// model
			// -----------------
			std::string modelName;
			if (m_animator && m_animator->GetModel())
				modelName = m_animator->GetModel()->GetName();

			{
				rapidjson::Value modelVal;
				modelVal.SetString(modelName.c_str(),
					static_cast<rapidjson::SizeType>(modelName.size()),
					alloc);
				out.AddMember("model", modelVal, alloc);
			}

			// -----------------
			// graph
			// -----------------
			rapidjson::Value graphVal(rapidjson::kObjectType);

			if (m_animationGraph)
			{
				// basic playback info
				graphVal.AddMember("playbackSpeed", m_animationGraph->playbackSpeed, alloc);
				graphVal.AddMember("playing", m_animationGraph->playing, alloc);
				graphVal.AddMember("currentTime", m_animationGraph->currentTime, alloc);

				// ----- states -----
				{
					rapidjson::Value statesArr(rapidjson::kArrayType);
					for (const auto& sPtr : m_animationGraph->states)
					{
						const AnimationStateNode& s = *sPtr;
						rapidjson::Value js(rapidjson::kObjectType);

						js.AddMember("id", s.id, alloc);

						rapidjson::Value nameVal;
						nameVal.SetString(s.name.c_str(),
							static_cast<rapidjson::SizeType>(s.name.size()),
							alloc);
						js.AddMember("name", nameVal, alloc);

						rapidjson::Value clipVal;
						clipVal.SetString(s.clipName.c_str(),
							static_cast<rapidjson::SizeType>(s.clipName.size()),
							alloc);
						js.AddMember("clipName", clipVal, alloc);

						js.AddMember("isStartState", s.isStartState, alloc);
						js.AddMember("isAttached", s.isAttached, alloc);
						js.AddMember("speed", s.speed, alloc);
						js.AddMember("blendWeight", s.blendWeight, alloc);

						// editorPos as [x, y]
						rapidjson::Value posArr(rapidjson::kArrayType);
						posArr.PushBack(s.editorPos.x, alloc);
						posArr.PushBack(s.editorPos.y, alloc);
						js.AddMember("editorPos", posArr, alloc);

						statesArr.PushBack(js, alloc);
					}
					graphVal.AddMember("states", statesArr, alloc);
				}

				// ----- transitions -----
				{
					rapidjson::Value transArr(rapidjson::kArrayType);
					for (const AnimationTransition& t : m_animationGraph->transitions)
					{
						rapidjson::Value jt(rapidjson::kObjectType);
						jt.AddMember("fromNodeId", t.fromNodeId, alloc);
						jt.AddMember("toNodeId", t.toNodeId, alloc);
						jt.AddMember("exitTime", t.exitTime, alloc);
						jt.AddMember("duration", t.duration, alloc);

						// conditions[]
						rapidjson::Value condArr(rapidjson::kArrayType);
						for (const AnimationCondition& c : t.conditions)
						{
							rapidjson::Value jc(rapidjson::kObjectType);

							// parameterName
							{
								rapidjson::Value pnameVal;
								pnameVal.SetString(c.parameterName.c_str(),
									static_cast<rapidjson::SizeType>(c.parameterName.size()),
									alloc);
								jc.AddMember("parameterName", pnameVal, alloc);
							}

							// comparison operator string (==, >, etc.)
							{
								rapidjson::Value compVal;
								compVal.SetString(c.comparison.c_str(),
									static_cast<rapidjson::SizeType>(c.comparison.size()),
									alloc);
								jc.AddMember("comparison", compVal, alloc);
							}

							jc.AddMember("threshold", c.threshold, alloc);
							jc.AddMember("boolValue", c.boolValue, alloc);

							condArr.PushBack(jc, alloc);
						}
						jt.AddMember("conditions", condArr, alloc);

						transArr.PushBack(jt, alloc);
					}
					graphVal.AddMember("transitions", transArr, alloc);
				}

				// ----- parameters -----
				{
					rapidjson::Value paramArr(rapidjson::kArrayType);
					for (const AnimationParameter& p : m_animationGraph->parameters)
					{
						rapidjson::Value jp(rapidjson::kObjectType);

						// name
						{
							rapidjson::Value pnameVal;
							pnameVal.SetString(p.name.c_str(),
								static_cast<rapidjson::SizeType>(p.name.size()),
								alloc);
							jp.AddMember("name", pnameVal, alloc);
						}

						// enum class Type = int
						jp.AddMember("type", static_cast<int>(p.type), alloc);

						jp.AddMember("boolValue", p.boolValue, alloc);
						jp.AddMember("floatValue", p.floatValue, alloc);
						jp.AddMember("intValue", p.intValue, alloc);
						jp.AddMember("triggerValue", p.triggerValue, alloc);

						paramArr.PushBack(jp, alloc);
					}
					graphVal.AddMember("parameters", paramArr, alloc);
				}
			}

			out.AddMember("graph", graphVal, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.IsObject()) return;

			// -------- model --------
			if (in.HasMember("model") && in["model"].IsString())
			{
				std::string modelName = in["model"].GetString();

				auto model = AssetManager::GetInstance().GetModel("../Resources/Models/" + modelName);
				if (model)
				{
					const aiScene* scene = model->GetAssimpScene();
					if (scene && scene->mNumAnimations > 0)
					{
						m_animator = std::make_shared<graphics::Animator>(model);
					}
				}
			}

			// Ensure graph exists
			if (!m_animationGraph)
				m_animationGraph = std::make_shared<AnimationGraph>();

			// Reset runtime data
			m_animationGraph->states.clear();
			m_animationGraph->links.clear();
			m_animationGraph->transitions.clear();
			m_animationGraph->parameters.clear();
			m_animationGraph->current.reset();
			m_animationGraph->currentTime = 0.0f;
			m_animationGraph->playing = false;
			m_animationGraph->playbackSpeed = 1.0f;

			// -------- graph --------
			if (!in.HasMember("graph") || !in["graph"].IsObject())
				return;

			const rapidjson::Value& g = in["graph"];

			// playback info
			if (g.HasMember("playbackSpeed") && g["playbackSpeed"].IsNumber())
				m_animationGraph->playbackSpeed = g["playbackSpeed"].GetFloat();

			if (g.HasMember("playing") && g["playing"].IsBool())
				m_animationGraph->playing = g["playing"].GetBool();

			if (g.HasMember("currentTime") && g["currentTime"].IsNumber())
				m_animationGraph->currentTime = g["currentTime"].GetFloat();

			// ----- states -----
			if (g.HasMember("states") && g["states"].IsArray())
			{
				const auto& arr = g["states"];
				for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
				{
					const auto& js = arr[i];
					auto s = std::make_shared<AnimationStateNode>();

					if (js.HasMember("id") && js["id"].IsInt())
						s->id = js["id"].GetInt();

					if (js.HasMember("name") && js["name"].IsString())
						s->name = js["name"].GetString();

					if (js.HasMember("clipName") && js["clipName"].IsString())
						s->clipName = js["clipName"].GetString();

					if (js.HasMember("isStartState") && js["isStartState"].IsBool())
						s->isStartState = js["isStartState"].GetBool();

					if (js.HasMember("isAttached") && js["isAttached"].IsBool())
						s->isAttached = js["isAttached"].GetBool();

					if (js.HasMember("speed") && js["speed"].IsNumber())
						s->speed = js["speed"].GetFloat();

					if (js.HasMember("blendWeight") && js["blendWeight"].IsNumber())
						s->blendWeight = js["blendWeight"].GetFloat();

					if (js.HasMember("editorPos") && js["editorPos"].IsArray() && js["editorPos"].Size() == 2)
					{
						s->editorPos.x = js["editorPos"][0].GetFloat();
						s->editorPos.y = js["editorPos"][1].GetFloat();
						ImNodes::SetNodeEditorSpacePos(s->id, s->editorPos);
					}
					else
					{
						s->editorPos = ImVec2{ 100.f, 100.f };
						ImNodes::SetNodeEditorSpacePos(s->id, s->editorPos);
					}

					m_animationGraph->states.push_back(s);

					// Pick start state as current
					if (s->isStartState)
						m_animationGraph->current = s;
				}
			}

			// ----- transitions -----
			if (g.HasMember("transitions") && g["transitions"].IsArray())
			{
				const auto& arr = g["transitions"];
				for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
				{
					const auto& jt = arr[i];
					AnimationTransition t{};

					if (jt.HasMember("fromNodeId") && jt["fromNodeId"].IsInt())
						t.fromNodeId = jt["fromNodeId"].GetInt();

					if (jt.HasMember("toNodeId") && jt["toNodeId"].IsInt())
						t.toNodeId = jt["toNodeId"].GetInt();

					if (jt.HasMember("exitTime") && jt["exitTime"].IsNumber())
						t.exitTime = jt["exitTime"].GetFloat();

					if (jt.HasMember("duration") && jt["duration"].IsNumber())
						t.duration = jt["duration"].GetFloat();

					// conditions[]
					if (jt.HasMember("conditions") && jt["conditions"].IsArray())
					{
						const auto& condArr = jt["conditions"];
						for (rapidjson::SizeType ci = 0; ci < condArr.Size(); ++ci)
						{
							const auto& jc = condArr[ci];
							AnimationCondition c{};

							if (jc.HasMember("parameterName") && jc["parameterName"].IsString())
								c.parameterName = jc["parameterName"].GetString();

							if (jc.HasMember("comparison") && jc["comparison"].IsString())
								c.comparison = jc["comparison"].GetString();

							if (jc.HasMember("threshold") && jc["threshold"].IsNumber())
								c.threshold = jc["threshold"].GetFloat();

							if (jc.HasMember("boolValue") && jc["boolValue"].IsBool())
								c.boolValue = jc["boolValue"].GetBool();

							t.conditions.push_back(c);
						}
					}

					m_animationGraph->transitions.push_back(t);

					// Rebuild editor link from transition
					AnimationLink link{};
					link.id = static_cast<int>(m_animationGraph->links.size()) + 1;
					link.fromNodeId = t.fromNodeId;
					link.toNodeId = t.toNodeId;
					m_animationGraph->links.push_back(link);
				}
			}

			// ----- parameters -----
			if (g.HasMember("parameters") && g["parameters"].IsArray())
			{
				const auto& arr = g["parameters"];
				for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
				{
					const auto& jp = arr[i];
					AnimationParameter p{};

					if (jp.HasMember("name") && jp["name"].IsString())
						p.name = jp["name"].GetString();

					if (jp.HasMember("type") && jp["type"].IsInt())
						p.type = static_cast<AnimationParameter::Type>(jp["type"].GetInt());

					if (jp.HasMember("boolValue") && jp["boolValue"].IsBool())
						p.boolValue = jp["boolValue"].GetBool();

					if (jp.HasMember("floatValue") && jp["floatValue"].IsNumber())
						p.floatValue = jp["floatValue"].GetFloat();

					if (jp.HasMember("intValue") && jp["intValue"].IsInt())
						p.intValue = jp["intValue"].GetInt();

					if (jp.HasMember("triggerValue") && jp["triggerValue"].IsBool())
						p.triggerValue = jp["triggerValue"].GetBool();

					m_animationGraph->parameters.push_back(p);
				}
			}
		}

		//XPROPERTY_DEF(
		//	"AnimationComponent", AnimationComponent,
		//	xproperty::obj_member<"animModelName", &AnimationComponent::animModelName>
		//)
	};

	/*!***********************************************************************
	\brief
	 State Machine component structure.
	*************************************************************************/
	class StateManager; // forward declaration
	struct StateMachine
	{
		StateManager* manager = nullptr;
		ScriptNode* m_CurrentScript = nullptr;
		ScriptNode* m_PreviousScript = nullptr;

		std::deque<std::shared_ptr<ScriptNode>> m_Nodes;
		std::vector<std::pair<int, int>> m_Links;
		//std::unordered_map<ScriptNode*, ScriptNode*> scriptTransitions;
		std::unordered_map<int, int> scriptTransitions;
		std::vector<int> m_History;
	public:
		/*!***********************************************************************
		\brief
		   Initialize FSM node and script instance for the entity.
		*************************************************************************/
		void Init(EntityID entity)
		{
			ScriptNode* startScript = nullptr;

			// Find node marked as start
			for (auto& n : m_Nodes)
			{
				if (n->isStartNode)
				{
					startScript = n.get();
					break;
				}
			}

			// Fallback to first node if none is marked
			if (!startScript && !m_Nodes.empty())
			{
				startScript = m_Nodes.front().get();
				//EE_CORE_INFO("FSM: No start node marked, using first node '%s' (id=%d)", startScript->name.c_str(), startScript->id);
			}

			// Assign and initialize
			m_CurrentScript = startScript;

			if (m_CurrentScript)
			{
				m_CurrentScript->CreateInstance(entity);
				m_CurrentScript->OnEnter();
				//EE_CORE_INFO("FSM: Initialized with start node '%s' (id=%d)",
				//	m_CurrentScript->name.c_str(), m_CurrentScript->id);
			}
			//else
			//{
			//	EE_CORE_WARN("FSM: Init() called but no valid start node found for entity %d!", entity);
			//}

			m_History.clear();
			m_PreviousScript = nullptr;
		}
		/*!***********************************************************************
		\brief
		   Update the current state of an entity.
		*************************************************************************/
		void Update(EntityID entity, float dt)
		{
			(void)dt;
			// Ensure current script exists and is valid
			if (!m_CurrentScript ||
				std::find_if(m_Nodes.begin(), m_Nodes.end(),
					[&](const std::shared_ptr<ScriptNode>& n) { return n.get() == m_CurrentScript; }) == m_Nodes.end())
			{
				//EE_CORE_WARN("FSM: Current script for entity %d invalid or deleted. Attempting recovery...", entity);

				// Try to find start node
				m_CurrentScript = nullptr;
				for (auto& n : m_Nodes)
				{
					if (n->isStartNode)
					{
						m_CurrentScript = n.get();
						//EE_CORE_INFO("FSM: Reset to start node '%s' (id=%d)", n->name.c_str(), n->id);
						break;
					}
				}

				// Fallback to first node if no start node
				if (!m_CurrentScript && !m_Nodes.empty())
				{
					m_CurrentScript = m_Nodes.front().get();
					//EE_CORE_INFO("FSM: Fallback to first node '%s' (id=%d)", m_CurrentScript->name.c_str(), m_CurrentScript->id);
				}

				// Initialize recovered node
				if (m_CurrentScript)
				{
					m_CurrentScript->CreateInstance(entity);
					m_CurrentScript->OnEnter();
				}
				else
				{
					//EE_CORE_WARN("FSM: No valid nodes found; skipping update.");
					return;
				}
			}

			// Run script update logic (C# handles transitions now)
			if (m_CurrentScript && m_CurrentScript->instance)
				m_CurrentScript->OnUpdate();
		}

		ScriptNode* FindNodeById(int id)
		{
			for (auto& n : m_Nodes)
				if (n && n->id == id)
					return n.get();
			return nullptr;
		}

		template <typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			using namespace rapidjson;

			out.SetObject();

			// =======================
			// 1) Serialize nodes
			// =======================
			Value nodes(kArrayType);

			for (const auto& nodePtr : m_Nodes)
			{
				if (!nodePtr)
					continue;

				Value n(kObjectType);

				// id
				n.AddMember("id", nodePtr->id, alloc);

				// name
				{
					Value nameVal;
					nameVal.SetString(nodePtr->name.c_str(),
						static_cast<SizeType>(nodePtr->name.size()),
						alloc);
					n.AddMember("name", nameVal, alloc);
				}

				// scriptClassName
				{
					Value scriptVal;
					scriptVal.SetString(nodePtr->scriptClassName.c_str(),
						static_cast<SizeType>(nodePtr->scriptClassName.size()),
						alloc);
					n.AddMember("scriptClassName", scriptVal, alloc);
				}

				// flags
				n.AddMember("isAttached", nodePtr->isAttached, alloc);
				n.AddMember("isStartNode", nodePtr->isStartNode, alloc);

				n.AddMember("posX", nodePtr->editorPosition.x, alloc);
				n.AddMember("posY", nodePtr->editorPosition.y, alloc);

				// NOTE: instance is runtime-only and NOT serialized.

				nodes.PushBack(n, alloc);
			}

			out.AddMember("nodes", nodes, alloc);

			// =======================
			// 2) Serialize links
			// =======================
			Value links(kArrayType);

			for (const auto& link : m_Links)
			{
				// In your editor:
				// output attr = nodeId*10
				// input  attr = nodeId*10 + 1
				const int fromNodeId = link.first / 10;
				const int toNodeId = (link.second - 1) / 10;

				Value l(kObjectType);
				l.AddMember("fromId", fromNodeId, alloc);
				l.AddMember("toId", toNodeId, alloc);
				links.PushBack(l, alloc);
			}

			out.AddMember("links", links, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			using namespace rapidjson;

			// Clear old state
			m_Nodes.clear();
			m_Links.clear();
			scriptTransitions.clear();
			m_CurrentScript = nullptr;
			m_PreviousScript = nullptr;
			m_History.clear();

			if (!in.IsObject())
				return;

			// =======================
			// 1) Recreate nodes
			// =======================
			std::unordered_map<int, ScriptNode*> idToNode;

			if (in.HasMember("nodes") && in["nodes"].IsArray())
			{
				const auto& nodes = in["nodes"];
				for (auto& nVal : nodes.GetArray())
				{
					if (!nVal.IsObject())
						continue;

					auto node = std::make_shared<ScriptNode>();

					// id
					if (nVal.HasMember("id") && nVal["id"].IsInt())
						node->id = nVal["id"].GetInt();

					// name
					if (nVal.HasMember("name") && nVal["name"].IsString())
						node->name = nVal["name"].GetString();

					// scriptClassName
					if (nVal.HasMember("scriptClassName") && nVal["scriptClassName"].IsString())
						node->scriptClassName = nVal["scriptClassName"].GetString();

					// isAttached
					if (nVal.HasMember("isAttached") && nVal["isAttached"].IsBool())
						node->isAttached = nVal["isAttached"].GetBool();

					// isStartNode
					if (nVal.HasMember("isStartNode") && nVal["isStartNode"].IsBool())
						node->isStartNode = nVal["isStartNode"].GetBool();

					if (nVal.HasMember("posX") && nVal["posX"].IsNumber())
						node->editorPosition.x = nVal["posX"].GetFloat();

					if (nVal.HasMember("posY") && nVal["posY"].IsNumber())
						node->editorPosition.y = nVal["posY"].GetFloat();

					node->positionInitialized = false;

					// instance is runtime-only; will be created via CreateInstance(entity)
					// when Init(entity) is called.

					idToNode[node->id] = node.get();
					m_Nodes.emplace_back(std::move(node));
				}
			}

			// =======================
			// 2) Recreate links + scriptTransitions
			// =======================
			if (in.HasMember("links") && in["links"].IsArray())
			{
				for (auto& lVal : in["links"].GetArray())
				{
					if (!lVal.IsObject())
						continue;

					if (!lVal.HasMember("fromId") || !lVal.HasMember("toId"))
						continue;

					if (!lVal["fromId"].IsInt() || !lVal["toId"].IsInt())
						continue;

					int rawFrom = lVal["fromId"].GetInt();
					int rawTo = lVal["toId"].GetInt();

					int fromNodeId = rawFrom;
					int toNodeId = rawTo;

					// --- Prefer interpreting as NODE ids ---
					const bool rawAreNodeIds =
						(idToNode.find(rawFrom) != idToNode.end()) &&
						(idToNode.find(rawTo) != idToNode.end());

					if (!rawAreNodeIds)
					{
						// --- Try old ATTR-id format ---
						const bool couldBeAttr =
							((rawFrom % 10) == 0) &&
							((rawTo % 10) == 1);

						if (!couldBeAttr)
							continue;

						int convFrom = rawFrom / 10;
						int convTo = (rawTo - 1) / 10;

						const bool convertedAreNodeIds =
							(idToNode.find(convFrom) != idToNode.end()) &&
							(idToNode.find(convTo) != idToNode.end());

						if (!convertedAreNodeIds)
							continue;

						fromNodeId = convFrom;
						toNodeId = convTo;
					}

					// --- Rebuild ImNodes attribute IDs ---
					const int fromAttr = fromNodeId * 10;      // output
					const int toAttr = toNodeId * 10 + 1;  // input

					m_Links.emplace_back(fromAttr, toAttr);

					// --- FSM transitions use NODE ids ---
					scriptTransitions[fromNodeId] = toNodeId;
				}
			}

			// manager pointer is not restored here; set it externally if needed.
			// Actual script instances will be created when you call Init(entity),
			// which finds the start node and calls CreateInstance + OnEnter().
		}

	};

	/*!***********************************************************************
	\brief
	 Nav Mesh component structure.
	*************************************************************************/
	struct NavMeshComponent
	{
		// Recast build config
		float cellSize = 0.05f;
		float cellHeight = 0.05f;
		float agentHeight = 1.0f;
		float agentRadius = 0.5f;
		float bakedAgentRadius = 0.0f;
		float bakedAgentHeight = 0.0f;
		float agentMaxClimb = 0.0f;
		float agentMaxSlope = 45.0f;

		// Debug toggles
		bool  drawInputTri = false;
		bool  drawWalkable = true;
		bool  drawNavMesh = false;

		// Recast transient build data
		struct BuildData;
		BuildData* build = nullptr;

		// Detour runtime
		struct Runtime
		{
			dtNavMesh* nav = nullptr;
			dtNavMeshQuery* query = nullptr;
			unsigned long long tileRef = 0;
			//dtTileRef tileRef = 0;
		};
		Runtime* runtime = nullptr;

		// -----------------------------
		// Persistent baked navmesh data
		// -----------------------------
		struct BakedTile
		{
			std::string dataB64;
			int dataSize = 0;
		};

		float bakedOrig[3]{ 0.0f, 0.0f, 0.0f };
		float bakedTileWidth = 0.0f;
		float bakedTileHeight = 0.0f;
		int bakedMaxTiles = 0;
		int bakedMaxPolys = 0;
		std::vector<BakedTile> bakedTiles;

		bool HasBaked() const { return !bakedTiles.empty(); }

		// Small header-only Base64 (so Serialize/Deserialize can use it without extra includes)
		static std::string Base64Encode(const unsigned char* data, size_t len)
		{
			static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string out;
			out.reserve(((len + 2) / 3) * 4);

			size_t i = 0;
			while (i < len)
			{
				uint32_t a = i < len ? data[i++] : 0;
				uint32_t b = i < len ? data[i++] : 0;
				uint32_t c = i < len ? data[i++] : 0;

				uint32_t triple = (a << 16) | (b << 8) | c;

				out.push_back(kB64[(triple >> 18) & 0x3F]);
				out.push_back(kB64[(triple >> 12) & 0x3F]);
				out.push_back(((i - 2) <= len) ? kB64[(triple >> 6) & 0x3F] : '=');
				out.push_back(((i - 1) <= len) ? kB64[(triple) & 0x3F] : '=');
			}

			const size_t mod = len % 3;
			if (mod == 1) { out[out.size() - 1] = '='; out[out.size() - 2] = '='; }
			else if (mod == 2) { out[out.size() - 1] = '='; }

			return out;
		}

		static bool Base64Decode(const char* b64, std::vector<unsigned char>& out)
		{
			if (!b64) return false;

			static int rev[256];
			static bool init = false;
			if (!init)
			{
				for (int i = 0; i < 256; ++i) rev[i] = -1;
				const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				for (int i = 0; i < 64; ++i) rev[(unsigned char)kB64[i]] = i;
				rev[(unsigned char)'='] = 0;
				init = true;
			}

			out.clear();
			int val = 0, valb = -8;
			for (const unsigned char* p = (const unsigned char*)b64; *p; ++p)
			{
				int d = rev[*p];
				if (d == -1) continue; // skip whitespace/invalid
				val = (val << 6) + d;
				valb += 6;
				if (valb >= 0)
				{
					out.push_back((unsigned char)((val >> valb) & 0xFF));
					valb -= 8;
				}
			}
			return !out.empty();
		}
		
		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("cellSize", cellSize, alloc);
			out.AddMember("cellHeight", cellHeight, alloc);
			out.AddMember("agentHeight", agentHeight, alloc);
			out.AddMember("agentRadius", agentRadius, alloc);
			out.AddMember("bakedAgentRadius", bakedAgentRadius, alloc);
			out.AddMember("bakedAgentHeight", bakedAgentHeight, alloc);
			out.AddMember("agentMaxClimb", agentMaxClimb, alloc);
			out.AddMember("agentMaxSlope", agentMaxSlope, alloc);

			out.AddMember("drawInputTri", drawInputTri, alloc);
			out.AddMember("drawWalkable", drawWalkable, alloc);
			out.AddMember("drawNavMesh", drawNavMesh, alloc);

			rapidjson::Value bakedObj(rapidjson::kObjectType);

			bakedObj.AddMember("origX", bakedOrig[0], alloc);
			bakedObj.AddMember("origY", bakedOrig[1], alloc);
			bakedObj.AddMember("origZ", bakedOrig[2], alloc);
			bakedObj.AddMember("tileWidth", bakedTileWidth, alloc);
			bakedObj.AddMember("tileHeight", bakedTileHeight, alloc);
			bakedObj.AddMember("maxTiles", bakedMaxTiles, alloc);
			bakedObj.AddMember("maxPolys", bakedMaxPolys, alloc);

			rapidjson::Value tilesArr(rapidjson::kArrayType);
			tilesArr.Reserve((rapidjson::SizeType)bakedTiles.size(), alloc);

			for (auto const& t : bakedTiles)
			{
				rapidjson::Value tObj(rapidjson::kObjectType);
				tObj.AddMember("size", t.dataSize, alloc);
				tObj.AddMember("data", rapidjson::Value(t.dataB64.c_str(), alloc), alloc);
				tilesArr.PushBack(tObj, alloc);
			}

			bakedObj.AddMember("tiles", tilesArr, alloc);
			out.AddMember("bakedNav", bakedObj, alloc);

		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.IsObject()) return;

			if (in.HasMember("cellSize")) cellSize = in["cellSize"].GetFloat();
			if (in.HasMember("cellHeight")) cellHeight = in["cellHeight"].GetFloat();
			if (in.HasMember("agentHeight")) agentHeight = in["agentHeight"].GetFloat();
			if (in.HasMember("agentRadius")) agentRadius = in["agentRadius"].GetFloat();
			if (in.HasMember("bakedAgentRadius")) bakedAgentRadius = in["bakedAgentRadius"].GetFloat();
			if (in.HasMember("bakedAgentHeight")) bakedAgentHeight = in["bakedAgentHeight"].GetFloat();
			if (in.HasMember("agentMaxClimb")) agentMaxClimb = in["agentMaxClimb"].GetFloat();
			if (in.HasMember("agentMaxSlope")) agentMaxSlope = in["agentMaxSlope"].GetFloat();

			if (in.HasMember("drawInputTri")) drawInputTri = in["drawInputTri"].GetBool();
			if (in.HasMember("drawWalkable")) drawWalkable = in["drawWalkable"].GetBool();
			if (in.HasMember("drawNavMesh")) drawNavMesh = in["drawNavMesh"].GetBool();

			bakedTiles.clear();

			if (in.HasMember("bakedNav") && in["bakedNav"].IsObject())
			{
				const auto& b = in["bakedNav"];

				if (b.HasMember("origX")) bakedOrig[0] = b["origX"].GetFloat();
				if (b.HasMember("origY")) bakedOrig[1] = b["origY"].GetFloat();
				if (b.HasMember("origZ")) bakedOrig[2] = b["origZ"].GetFloat();
				if (b.HasMember("tileWidth")) bakedTileWidth = b["tileWidth"].GetFloat();
				if (b.HasMember("tileHeight")) bakedTileHeight = b["tileHeight"].GetFloat();
				if (b.HasMember("maxTiles")) bakedMaxTiles = b["maxTiles"].GetInt();
				if (b.HasMember("maxPolys")) bakedMaxPolys = b["maxPolys"].GetInt();

				if (b.HasMember("tiles") && b["tiles"].IsArray())
				{
					for (auto& tv : b["tiles"].GetArray())
					{
						if (!tv.IsObject()) continue;

						BakedTile t{};
						if (tv.HasMember("size")) t.dataSize = tv["size"].GetInt();
						if (tv.HasMember("data") && tv["data"].IsString()) t.dataB64 = tv["data"].GetString();
						bakedTiles.push_back(std::move(t));
					}
				}
			}

			// reset runtime-only
			build = nullptr;
			runtime = nullptr;
		}
	};

	/*!***********************************************************************
	\brief
	 Nav Mesh Agent component structure.
	*************************************************************************/
	struct NavMeshAgent
	{
		float speed = 3.0f;
		float acceleration = 8.0f;
		float stoppingDistance = 0.2f;
		bool autoRotate = true;
		bool debugDrawPath = true;

		float radius = 0.5f;
		float height = 1.0f;
		float centerYOffset = 0.0f;

		bool autoFitFromCollider = true;
		bool didAutoFit = false;

		bool hasPath = false;
		Vec3 destination{};
		std::vector<Vec3> path;
		size_t currentCorner = 0;

		unsigned long long startPoly = 0;
		unsigned long long endPoly = 0;

		// jump
		bool navPaused = false;
		bool isJumping = false;

		Vec3 jumpStart;
		Vec3 jumpTarget;

		float jumpTimer = 0.0f;
		float jumpDuration = 0.4f;
		float jumpHeight = 1.0f;

		Vec3 lastDestination;
		Vec3 postJumpDestination = Ermine::Vec3{ 0.0f, 0.0f, 0.0f };
		bool hasPostJumpDestination = false;

		EntityID lastJumpFromNavMesh = 0;

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();

			out.AddMember("speed", speed, alloc);
			out.AddMember("acceleration", acceleration, alloc);
			out.AddMember("stoppingDistance", stoppingDistance, alloc);
			out.AddMember("autoRotate", autoRotate, alloc);
			out.AddMember("debugDrawPath", debugDrawPath, alloc);

			out.AddMember("radius", radius, alloc);
			out.AddMember("height", height, alloc);
			out.AddMember("centerYOffset", centerYOffset, alloc);

			out.AddMember("autoFitFromCollider", autoFitFromCollider, alloc);

			// Optional: persist destination (NOT the computed path)
			out.AddMember("destination", Ermine::Vec3ToJson(destination, alloc), alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.IsObject()) return;

			if (in.HasMember("speed")) speed = in["speed"].GetFloat();
			if (in.HasMember("acceleration")) acceleration = in["acceleration"].GetFloat();
			if (in.HasMember("stoppingDistance")) stoppingDistance = in["stoppingDistance"].GetFloat();
			if (in.HasMember("autoRotate")) autoRotate = in["autoRotate"].GetBool();
			if (in.HasMember("debugDrawPath")) debugDrawPath = in["debugDrawPath"].GetBool();

			if (in.HasMember("radius")) radius = in["radius"].GetFloat();
			if (in.HasMember("height")) height = in["height"].GetFloat();
			if (in.HasMember("centerYOffset")) centerYOffset = in["centerYOffset"].GetFloat();

			if (in.HasMember("autoFitFromCollider")) autoFitFromCollider = in["autoFitFromCollider"].GetBool();
			if (in.HasMember("destination")) destination = Ermine::JsonToVec3(in["destination"]);

			// reset runtime-only
			didAutoFit = false;
			hasPath = false;
			path.clear();
			currentCorner = 0;
			startPoly = 0;
			endPoly = 0;

			navPaused = false;
			isJumping = false;
			jumpTimer = 0.0f;

			postJumpDestination = Vec3{ 0.0f, 0.0f, 0.0f };
			hasPostJumpDestination = false;
			lastJumpFromNavMesh = 0;
		}
	};

	struct NavJumpLink
	{
		Ermine::Vec3 landingPosition; // where the agent should land
		float jumpDuration = 1.0f; // seconds
		float jumpHeight = 5.0f; // purely visual

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			out.AddMember("landingPosition", Ermine::Vec3ToJson(landingPosition, alloc), alloc);
			out.AddMember("jumpDuration", jumpDuration, alloc);
			out.AddMember("jumpHeight", jumpHeight, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			if (!in.IsObject()) return;
			if (in.HasMember("landingPosition")) landingPosition = Ermine::JsonToVec3(in["landingPosition"]);
			if (in.HasMember("jumpDuration")) jumpDuration = in["jumpDuration"].GetFloat();
			if (in.HasMember("jumpHeight")) jumpHeight = in["jumpHeight"].GetFloat();
		}
	};

	/*!***********************************************************************
	\brief
	 AABB component for caching bounding boxes - used for frustum culling optimization
	*************************************************************************/
	struct AABBComponent
	{
		AABB worldAABB;      // Cached world-space AABB
		bool isDirty = true; // True if transform changed since last calculation

		AABBComponent() = default;
		explicit AABBComponent(const AABB& box) : worldAABB(box), isDirty(false) {}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const {
			out.SetObject();
			// Don't serialize AABB - it gets recalculated from mesh/transform
			out.AddMember("isDirty", isDirty, alloc);
		}

		void Deserialize(const rapidjson::Value& in) {
			// Don't deserialize AABB - force recalculation
			isDirty = true;
			(void)in;
		}

		XPROPERTY_DEF(
			"AABBComponent", AABBComponent,
			xproperty::obj_member<"isDirty", &AABBComponent::isDirty>
		)
	};

	/*!***********************************************************************
	\brief
	  Health component for player/entity stats
	*************************************************************************/
	struct HealthComponent
	{
		float maxHealth = 100.0f;
		float currentHealth = 100.0f;
		float healthRegenRate = 0.0f; // Health regenerated per second
		bool isDead = false;

		HealthComponent() = default;
		explicit HealthComponent(float max) : maxHealth(max), currentHealth(max) {}

		void TakeDamage(float damage)
		{
			currentHealth = std::max(0.0f, currentHealth - damage);
			if (currentHealth <= 0.0f)
			{
				isDead = true;
			}
		}

		void Heal(float amount)
		{
			currentHealth = std::min(maxHealth, currentHealth + amount);
			if (currentHealth > 0.0f)
			{
				isDead = false;
			}
		}

		float GetHealthPercentage() const
		{
			return maxHealth > 0.0f ? (currentHealth / maxHealth) : 0.0f;
		}

		void Update(float dt)
		{
			if (healthRegenRate > 0.0f && currentHealth < maxHealth)
			{
				Heal(healthRegenRate * dt);
			}
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("maxHealth", maxHealth, alloc);
			out.AddMember("currentHealth", currentHealth, alloc);
			out.AddMember("healthRegenRate", healthRegenRate, alloc);
			out.AddMember("isDead", isDead, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("maxHealth") && in["maxHealth"].IsNumber())
				maxHealth = in["maxHealth"].GetFloat();
			if (in.HasMember("currentHealth") && in["currentHealth"].IsNumber())
				currentHealth = in["currentHealth"].GetFloat();
			if (in.HasMember("healthRegenRate") && in["healthRegenRate"].IsNumber())
				healthRegenRate = in["healthRegenRate"].GetFloat();
			if (in.HasMember("isDead") && in["isDead"].IsBool())
				isDead = in["isDead"].GetBool();
		}

		XPROPERTY_DEF(
			"HealthComponent", HealthComponent,
			xproperty::obj_member<"maxHealth", &HealthComponent::maxHealth>,
			xproperty::obj_member<"currentHealth", &HealthComponent::currentHealth>,
			xproperty::obj_member<"healthRegenRate", &HealthComponent::healthRegenRate>,
			xproperty::obj_member<"isDead", &HealthComponent::isDead>
		)
	};

	/*!***********************************************************************
	\brief
	  Skill slot data for skills UI
	*************************************************************************/
	struct SkillSlot
	{
		std::string skillName = "";
		std::string iconPath = "";
		float cooldownTime = 0.0f;     // Total cooldown duration
		float currentCooldown = 0.0f;  // Current cooldown remaining
		bool isActive = false;
		int keyBinding = -1;           // Key code for activation

		bool IsOnCooldown() const { return currentCooldown > 0.0f; }
		float GetCooldownPercentage() const
		{
			return cooldownTime > 0.0f ? (currentCooldown / cooldownTime) : 0.0f;
		}

		void Activate()
		{
			if (!IsOnCooldown())
			{
				isActive = true;
				currentCooldown = cooldownTime;
			}
		}

		void Update(float dt)
		{
			if (currentCooldown > 0.0f)
			{
				currentCooldown = std::max(0.0f, currentCooldown - dt);
			}
		}
	};

	/*!***********************************************************************
	\brief
	  Skills component for managing player abilities
	*************************************************************************/
	struct SkillsComponent
	{
		static constexpr int MAX_SKILLS = 6;
		std::array<SkillSlot, MAX_SKILLS> skills;

		SkillsComponent() = default;

		void Update(float dt)
		{
			for (auto& skill : skills)
			{
				skill.Update(dt);
			}
		}

		bool ActivateSkill(int index)
		{
			if (index >= 0 && index < MAX_SKILLS)
			{
				skills[index].Activate();
				return true;
			}
			return false;
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			rapidjson::Value skillsArray(rapidjson::kArrayType);

			for (const auto& skill : skills)
			{
				rapidjson::Value skillObj(rapidjson::kObjectType);
				rapidjson::Value nameVal;
				nameVal.SetString(skill.skillName.c_str(), static_cast<rapidjson::SizeType>(skill.skillName.size()), alloc);
				skillObj.AddMember("skillName", nameVal, alloc);

				rapidjson::Value iconVal;
				iconVal.SetString(skill.iconPath.c_str(), static_cast<rapidjson::SizeType>(skill.iconPath.size()), alloc);
				skillObj.AddMember("iconPath", iconVal, alloc);

				skillObj.AddMember("cooldownTime", skill.cooldownTime, alloc);
				skillObj.AddMember("keyBinding", skill.keyBinding, alloc);

				skillsArray.PushBack(skillObj, alloc);
			}

			out.AddMember("skills", skillsArray, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("skills") && in["skills"].IsArray())
			{
				const auto& skillsArray = in["skills"].GetArray();
				int index = 0;
				for (auto& skillVal : skillsArray)
				{
					if (index >= MAX_SKILLS) break;

					if (skillVal.HasMember("skillName") && skillVal["skillName"].IsString())
						skills[index].skillName = skillVal["skillName"].GetString();
					if (skillVal.HasMember("iconPath") && skillVal["iconPath"].IsString())
						skills[index].iconPath = skillVal["iconPath"].GetString();
					if (skillVal.HasMember("cooldownTime") && skillVal["cooldownTime"].IsNumber())
						skills[index].cooldownTime = skillVal["cooldownTime"].GetFloat();
					if (skillVal.HasMember("keyBinding") && skillVal["keyBinding"].IsInt())
						skills[index].keyBinding = skillVal["keyBinding"].GetInt();

					index++;
				}
			}
		}

		XPROPERTY_DEF(
			"SkillsComponent", SkillsComponent
		)
	};

	/*!***********************************************************************
	\brief
	  UI Button component for clickable menu buttons
	*************************************************************************/
	struct UIButtonComponent
	{
		enum class ButtonAction
		{
			None,
			LoadScene,
			Quit,
			Custom
		};

		// Button visual properties
		std::string text = "";
		Vec3 position = { 0.5f, 0.5f, 0.0f };  // Normalized screen position
		Vec2 size = { 0.12f, 0.12f };           // Normalized screen size

		// Button state colors
		Vec3 normalColor = { 0.3f, 0.3f, 0.3f };
		Vec3 hoverColor = { 0.5f, 0.5f, 0.5f };
		Vec3 pressedColor = { 0.7f, 0.7f, 0.7f };
		Vec3 textColor = { 1.0f, 1.0f, 1.0f };
		float textScale = 1.0f;
		float backgroundAlpha = 1.0f;  // Button background transparency (0.0 = invisible, 1.0 = opaque)

		// Button state images (optional - if set, overrides color-based rendering)
		std::string normalImage = "";   // Image shown in normal state
		std::string hoverImage = "";    // Image shown when hovered (falls back to normalImage if empty)
		std::string pressedImage = "";  // Image shown when pressed (falls back to hoverImage if empty)

		// Button action
		ButtonAction action = ButtonAction::None;
		std::string actionData = "";  // Scene path for LoadScene, custom event name, etc.

		// Audio settings
		std::string hoverSoundName = "";  // Sound to play on hover
		std::string clickSoundName = "";  // Sound to play on click
		float soundVolume = 1.0f;         // Volume for button sounds (0.0 - 1.0)

		// Button state (runtime - don't serialize)
		bool isHovered = false;
		bool isPressed = false;

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			rapidjson::Value textVal(text.c_str(), alloc);
			out.AddMember("text", textVal, alloc);
			out.AddMember("position", Vec3ToJson(position, alloc), alloc);

			rapidjson::Value sizeVal(rapidjson::kArrayType);
			sizeVal.PushBack(size.x, alloc);
			sizeVal.PushBack(size.y, alloc);
			out.AddMember("size", sizeVal, alloc);

			out.AddMember("normalColor", Vec3ToJson(normalColor, alloc), alloc);
			out.AddMember("hoverColor", Vec3ToJson(hoverColor, alloc), alloc);
			out.AddMember("pressedColor", Vec3ToJson(pressedColor, alloc), alloc);
			out.AddMember("textColor", Vec3ToJson(textColor, alloc), alloc);
			out.AddMember("textScale", textScale, alloc);
			out.AddMember("backgroundAlpha", backgroundAlpha, alloc);

			// Button state images
			rapidjson::Value normalImageVal(normalImage.c_str(), alloc);
			out.AddMember("normalImage", normalImageVal, alloc);
			rapidjson::Value hoverImageVal(hoverImage.c_str(), alloc);
			out.AddMember("hoverImage", hoverImageVal, alloc);
			rapidjson::Value pressedImageVal(pressedImage.c_str(), alloc);
			out.AddMember("pressedImage", pressedImageVal, alloc);

			out.AddMember("action", static_cast<int>(action), alloc);
			rapidjson::Value actionDataVal(actionData.c_str(), alloc);
			out.AddMember("actionData", actionDataVal, alloc);

			// Audio settings
			rapidjson::Value hoverSoundVal(hoverSoundName.c_str(), alloc);
			out.AddMember("hoverSoundName", hoverSoundVal, alloc);
			rapidjson::Value clickSoundVal(clickSoundName.c_str(), alloc);
			out.AddMember("clickSoundName", clickSoundVal, alloc);
			out.AddMember("soundVolume", soundVolume, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("text") && in["text"].IsString())
				text = in["text"].GetString();
			if (in.HasMember("position") && in["position"].IsArray())
				position = JsonToVec3(in["position"]);
			if (in.HasMember("size") && in["size"].IsArray())
			{
				const auto& arr = in["size"].GetArray();
				if (arr.Size() >= 2)
				{
					size.x = arr[0].GetFloat();
					size.y = arr[1].GetFloat();
				}
			}
			if (in.HasMember("normalColor") && in["normalColor"].IsArray())
				normalColor = JsonToVec3(in["normalColor"]);
			if (in.HasMember("hoverColor") && in["hoverColor"].IsArray())
				hoverColor = JsonToVec3(in["hoverColor"]);
			if (in.HasMember("pressedColor") && in["pressedColor"].IsArray())
				pressedColor = JsonToVec3(in["pressedColor"]);
			if (in.HasMember("textColor") && in["textColor"].IsArray())
				textColor = JsonToVec3(in["textColor"]);
			if (in.HasMember("textScale") && in["textScale"].IsNumber())
				textScale = in["textScale"].GetFloat();
			if (in.HasMember("backgroundAlpha") && in["backgroundAlpha"].IsNumber())
				backgroundAlpha = in["backgroundAlpha"].GetFloat();

			// Button state images
			if (in.HasMember("normalImage") && in["normalImage"].IsString())
				normalImage = in["normalImage"].GetString();
			if (in.HasMember("hoverImage") && in["hoverImage"].IsString())
				hoverImage = in["hoverImage"].GetString();
			if (in.HasMember("pressedImage") && in["pressedImage"].IsString())
				pressedImage = in["pressedImage"].GetString();

			if (in.HasMember("action") && in["action"].IsInt())
				action = static_cast<ButtonAction>(in["action"].GetInt());
			if (in.HasMember("actionData") && in["actionData"].IsString())
				actionData = in["actionData"].GetString();

			// Audio settings
			if (in.HasMember("hoverSoundName") && in["hoverSoundName"].IsString())
				hoverSoundName = in["hoverSoundName"].GetString();
			if (in.HasMember("clickSoundName") && in["clickSoundName"].IsString())
				clickSoundName = in["clickSoundName"].GetString();
			if (in.HasMember("soundVolume") && in["soundVolume"].IsNumber())
				soundVolume = in["soundVolume"].GetFloat();
		}

		XPROPERTY_DEF("UIButtonComponent", UIButtonComponent)
	};

	/*!***********************************************************************
	\brief
	  UI Slider component for volume controls and other adjustable values
	*************************************************************************/
	struct UISliderComponent
	{
		enum class SliderTarget
		{
			None,
			MasterVolume,
			MusicVolume,
			SFXVolume,
			AmbienceVolume,
			Custom
		};

		// Slider visual properties
		Vec3 position = { 0.5f, 0.5f, 0.0f };  // Normalized screen position (center of slider)
		Vec2 size = { 0.2f, 0.03f };           // Normalized screen size (width, height)

		// Slider colors
		Vec3 trackColor = { 0.2f, 0.2f, 0.2f };      // Background track color
		Vec3 fillColor = { 0.4f, 0.6f, 0.9f };       // Filled portion color
		Vec3 handleColor = { 1.0f, 1.0f, 1.0f };     // Handle/knob color
		Vec3 handleHoverColor = { 0.9f, 0.9f, 0.5f }; // Handle color when hovered
		float trackAlpha = 0.9f;
		float handleSize = 0.04f;  // Handle diameter (normalized)

		// Slider images (optional - overrides color-based rendering)
		std::string trackImage = "";   // Background track image
		std::string fillImage = "";    // Fill bar image
		std::string handleImage = "";  // Handle/knob image

		// Slider value
		float value = 1.0f;      // Current value (0.0 - 1.0)
		float minValue = 0.0f;   // Minimum value
		float maxValue = 1.0f;   // Maximum value

		// Slider target (what this slider controls)
		SliderTarget target = SliderTarget::None;
		std::string customTarget = "";  // For Custom target type

		// Label
		std::string label = "";
		Vec3 labelColor = { 1.0f, 1.0f, 1.0f };
		float labelScale = 0.8f;
		Vec2 labelOffset = { 0.0f, 0.04f };  // Offset from slider center

		// Label images (optional - shows image next to slider label)
		std::string labelImagePath = "";        // Normal/unselected image
		std::string labelActiveImagePath = "";  // Active/selected image (shown when dragging)

		// State (runtime - don't serialize)
		bool isHovered = false;
		bool isDragging = false;

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("position", Vec3ToJson(position, alloc), alloc);

			rapidjson::Value sizeVal(rapidjson::kArrayType);
			sizeVal.PushBack(size.x, alloc);
			sizeVal.PushBack(size.y, alloc);
			out.AddMember("size", sizeVal, alloc);

			out.AddMember("trackColor", Vec3ToJson(trackColor, alloc), alloc);
			out.AddMember("fillColor", Vec3ToJson(fillColor, alloc), alloc);
			out.AddMember("handleColor", Vec3ToJson(handleColor, alloc), alloc);
			out.AddMember("handleHoverColor", Vec3ToJson(handleHoverColor, alloc), alloc);
			out.AddMember("trackAlpha", trackAlpha, alloc);
			out.AddMember("handleSize", handleSize, alloc);

			rapidjson::Value trackImageVal(trackImage.c_str(), alloc);
			out.AddMember("trackImage", trackImageVal, alloc);
			rapidjson::Value fillImageVal(fillImage.c_str(), alloc);
			out.AddMember("fillImage", fillImageVal, alloc);
			rapidjson::Value handleImageVal(handleImage.c_str(), alloc);
			out.AddMember("handleImage", handleImageVal, alloc);

			out.AddMember("value", value, alloc);
			out.AddMember("minValue", minValue, alloc);
			out.AddMember("maxValue", maxValue, alloc);

			out.AddMember("target", static_cast<int>(target), alloc);
			rapidjson::Value customTargetVal(customTarget.c_str(), alloc);
			out.AddMember("customTarget", customTargetVal, alloc);

			rapidjson::Value labelVal(label.c_str(), alloc);
			out.AddMember("label", labelVal, alloc);
			out.AddMember("labelColor", Vec3ToJson(labelColor, alloc), alloc);
			out.AddMember("labelScale", labelScale, alloc);

			rapidjson::Value labelOffsetVal(rapidjson::kArrayType);
			labelOffsetVal.PushBack(labelOffset.x, alloc);
			labelOffsetVal.PushBack(labelOffset.y, alloc);
			out.AddMember("labelOffset", labelOffsetVal, alloc);

			rapidjson::Value labelImagePathVal(labelImagePath.c_str(), alloc);
			out.AddMember("labelImagePath", labelImagePathVal, alloc);
			rapidjson::Value labelActiveImagePathVal(labelActiveImagePath.c_str(), alloc);
			out.AddMember("labelActiveImagePath", labelActiveImagePathVal, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("position") && in["position"].IsArray())
				position = JsonToVec3(in["position"]);
			if (in.HasMember("size") && in["size"].IsArray())
			{
				const auto& arr = in["size"].GetArray();
				if (arr.Size() >= 2)
				{
					size.x = arr[0].GetFloat();
					size.y = arr[1].GetFloat();
				}
			}
			if (in.HasMember("trackColor") && in["trackColor"].IsArray())
				trackColor = JsonToVec3(in["trackColor"]);
			if (in.HasMember("fillColor") && in["fillColor"].IsArray())
				fillColor = JsonToVec3(in["fillColor"]);
			if (in.HasMember("handleColor") && in["handleColor"].IsArray())
				handleColor = JsonToVec3(in["handleColor"]);
			if (in.HasMember("handleHoverColor") && in["handleHoverColor"].IsArray())
				handleHoverColor = JsonToVec3(in["handleHoverColor"]);
			if (in.HasMember("trackAlpha") && in["trackAlpha"].IsNumber())
				trackAlpha = in["trackAlpha"].GetFloat();
			if (in.HasMember("handleSize") && in["handleSize"].IsNumber())
				handleSize = in["handleSize"].GetFloat();

			if (in.HasMember("trackImage") && in["trackImage"].IsString())
				trackImage = in["trackImage"].GetString();
			if (in.HasMember("fillImage") && in["fillImage"].IsString())
				fillImage = in["fillImage"].GetString();
			if (in.HasMember("handleImage") && in["handleImage"].IsString())
				handleImage = in["handleImage"].GetString();

			if (in.HasMember("value") && in["value"].IsNumber())
				value = in["value"].GetFloat();
			if (in.HasMember("minValue") && in["minValue"].IsNumber())
				minValue = in["minValue"].GetFloat();
			if (in.HasMember("maxValue") && in["maxValue"].IsNumber())
				maxValue = in["maxValue"].GetFloat();

			if (in.HasMember("target") && in["target"].IsInt())
				target = static_cast<SliderTarget>(in["target"].GetInt());
			if (in.HasMember("customTarget") && in["customTarget"].IsString())
				customTarget = in["customTarget"].GetString();

			if (in.HasMember("label") && in["label"].IsString())
				label = in["label"].GetString();
			if (in.HasMember("labelColor") && in["labelColor"].IsArray())
				labelColor = JsonToVec3(in["labelColor"]);
			if (in.HasMember("labelScale") && in["labelScale"].IsNumber())
				labelScale = in["labelScale"].GetFloat();
			if (in.HasMember("labelOffset") && in["labelOffset"].IsArray())
			{
				const auto& arr = in["labelOffset"].GetArray();
				if (arr.Size() >= 2)
				{
					labelOffset.x = arr[0].GetFloat();
					labelOffset.y = arr[1].GetFloat();
				}
			}
			if (in.HasMember("labelImagePath") && in["labelImagePath"].IsString())
				labelImagePath = in["labelImagePath"].GetString();
			if (in.HasMember("labelActiveImagePath") && in["labelActiveImagePath"].IsString())
				labelActiveImagePath = in["labelActiveImagePath"].GetString();
		}

		XPROPERTY_DEF("UISliderComponent", UISliderComponent)
	};

	/*!***********************************************************************
	\brief
	  UI configuration component for HUD elements
	*************************************************************************/
	struct UIComponent
	{
		// Healthbar settings
		bool showHealthbar = true;
		Ermine::Vec3 healthbarColor = { 0.85f, 0.15f, 0.15f };      // Red for health
		Ermine::Vec3 healthbarBgColor = { 0.2f, 0.2f, 0.2f };       // Dark gray
		float healthbarWidth = 0.30f;  // Percentage of screen width (increased for visibility)
		float healthbarHeight = 0.03f; // Percentage of screen height (increased for visibility)
		Ermine::Vec3 healthbarPosition = { 0.02f, 0.92f, 0.0f };  // Top-left corner, slightly lower
		std::string healthbarBgTexture = "";      // Background texture path (optional)
		std::string healthbarFillTexture = "";    // Fill texture path (optional)
		std::string healthbarFrameTexture = "";   // Frame texture path (optional)

		// Book Counter settings
		bool showBookCounter = true;
		int booksCollected = 0;
		int totalBooks = 4;
		Ermine::Vec3 bookCounterPosition = { 0.95f, 0.93f, 0.0f };  // Top-right, beside healthbar

		// Skills UI settings
		bool showSkills = true;
		float skillSlotSize = 0.06f;   // Percentage of screen size
		float skillSlotSpacing = 0.01f;
		Ermine::Vec3 skillsPosition = { 0.5f, 0.1f, 0.0f };  // Center bottom

		// Skill icon appearance states
		Ermine::Vec3 skillReadyTint = { 1.0f, 1.0f, 1.0f };         // Tint when skill is ready
		float skillReadyAlpha = 1.0f;                               // Alpha when skill is ready
		Ermine::Vec3 skillCooldownTint = { 0.5f, 0.5f, 0.5f };      // Tint when on cooldown
		float skillCooldownAlpha = 0.6f;                            // Alpha when on cooldown
		Ermine::Vec3 skillLowHealthTint = { 0.7f, 0.7f, 0.7f };     // Tint when insufficient health
		float skillLowHealthAlpha = 0.7f;                           // Alpha when insufficient health
		Ermine::Vec3 skillFallbackColor = { 0.3f, 0.3f, 0.3f };     // Color when no icon texture
		float skillFallbackAlpha = 0.5f;                            // Alpha when no icon texture

		// Skill cooldown overlay
		Ermine::Vec3 skillCooldownOverlayColor = { 0.0f, 0.0f, 0.0f };  // Cooldown radial overlay color
		float skillCooldownOverlayAlpha = 0.7f;                         // Cooldown radial overlay alpha

		// Skill activation flash effect
		float skillFlashDuration = 0.2f;                            // Flash effect duration (seconds)
		Ermine::Vec3 skillFlashColor = { 1.0f, 1.0f, 0.8f };        // Flash color (white-yellow)
		float skillFlashGlowSize = 0.02f;                           // Glow size increase beyond slot

		// Skill keybind label
		float skillKeybindTextScale = 0.6f;                         // Text scale for keybind labels
		float skillKeybindOffsetY = 0.02f;                          // Distance below slot
		Ermine::Vec3 skillKeybindColor = { 1.0f, 1.0f, 1.0f };      // Label text color
		float skillKeybindAlphaReady = 1.0f;                        // Alpha when skill ready
		float skillKeybindAlphaNotReady = 0.6f;                     // Alpha when skill not ready

		// Crosshair settings
		bool showCrosshair = true;
		Ermine::Vec3 crosshairColor = { 0.95f, 0.95f, 0.95f };  // Bright white for maximum visibility
		float crosshairSize = 0.05f;   // Reduced size for better precision
		float crosshairThickness = 0.001f;  // Thinner and sharper
		int crosshairStyle = 0;        // 0 = sniper scope, 1 = dot, 2 = circle
		float crosshairGap = 0.004f;   // Small center gap for precise aiming

		// Health system (Life Essence)
		float currentHealth = 50.0f;
		float maxHealth = 50.0f;
		float healthRegenRate = 5.0f;          // Health per second when regenerating
		float healthRegenDelay = 3.0f;         // Delay after skill use before regen starts
		float healthRegenTimer = 0.0f;         // Internal timer (don't serialize)

		// Mana bar settings
		bool showManaBar = false;      // Disabled - using health bar only
		float currentMana = 100.0f;
		float maxMana = 100.0f;
		float manaRegenRate = 10.0f;          // Mana per second
		float manaRegenDelay = 2.0f;          // Delay after skill use before regen starts
		float manaRegenTimer = 0.0f;          // Internal timer (don't serialize)
		Ermine::Vec3 manaBarColor = { 0.0f, 0.5f, 1.0f };       // Blue
		Ermine::Vec3 manaBarBgColor = { 0.2f, 0.2f, 0.3f };     // Dark blue-gray
		float manaBarWidth = 0.3f;            // Percentage of screen width
		float manaBarHeight = 0.03f;          // Percentage of screen height
		Ermine::Vec3 manaBarPosition = { 0.1f, 0.85f, 0.0f };   // Below health bar

		float GetHealth() const
		{
			return currentHealth;
		}

		void SetHealth(float value)
		{
			currentHealth = std::clamp(value, 0.0f, maxHealth);
		}

		// Skill slot data
		struct SkillSlot
		{
			float currentCooldown = 0.0f;     // Current cooldown remaining (seconds)
			float maxCooldown = 5.0f;         // Total cooldown duration
			float manaCost = 20.0f;           // Life essence cost to cast
			bool isOnCooldown = false;        // Is skill currently on cooldown?
			float activationFlashTimer = 0.0f; // Flash effect duration when skill is activated
			Ermine::Vec3 slotColor = { 0.25f, 0.25f, 0.25f };        // Dark gray background
			Ermine::Vec3 readyColor = { 0.85f, 0.85f, 0.85f };       // Light gray when ready
			Ermine::Vec3 cooldownColor = { 0.45f, 0.45f, 0.45f };    // Medium gray during cooldown
			Ermine::Vec3 cooldownOverlayColor = { 0.15f, 0.15f, 0.15f };  // Dark overlay during cooldown

			// Skill icon texture (optional - leave empty for solid color)
			std::string iconTexturePath = ""; // Path to skill icon image (PNG, JPG, DDS)

			// Skill information (for tooltips and keybind display)
			std::string skillName = "";       // e.g., "Shoot Orb"
			std::string keyBinding = "";      // e.g., "LMB", "RMB", "R"
			std::string description = "";     // e.g., "Fires an essence orb forward"
		};
		std::array<SkillSlot, 4> skills;      // 4 skill slots (Mouse1, Mouse1-teleport, Mouse2, R)

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showHealthbar", showHealthbar, alloc);
			out.AddMember("healthbarColor", Vec3ToJson(healthbarColor, alloc), alloc);
			out.AddMember("healthbarBgColor", Vec3ToJson(healthbarBgColor, alloc), alloc);
			out.AddMember("healthbarWidth", healthbarWidth, alloc);
			out.AddMember("healthbarHeight", healthbarHeight, alloc);
			out.AddMember("healthbarPosition", Vec3ToJson(healthbarPosition, alloc), alloc);
			rapidjson::Value bgTexVal(healthbarBgTexture.c_str(), alloc);
			out.AddMember("healthbarBgTexture", bgTexVal, alloc);
			rapidjson::Value fillTexVal(healthbarFillTexture.c_str(), alloc);
			out.AddMember("healthbarFillTexture", fillTexVal, alloc);
			rapidjson::Value frameTexVal(healthbarFrameTexture.c_str(), alloc);
			out.AddMember("healthbarFrameTexture", frameTexVal, alloc);

			out.AddMember("showBookCounter", showBookCounter, alloc);
			out.AddMember("booksCollected", booksCollected, alloc);
			out.AddMember("totalBooks", totalBooks, alloc);
			out.AddMember("bookCounterPosition", Vec3ToJson(bookCounterPosition, alloc), alloc);

			out.AddMember("showSkills", showSkills, alloc);
			out.AddMember("skillSlotSize", skillSlotSize, alloc);
			out.AddMember("skillSlotSpacing", skillSlotSpacing, alloc);
			out.AddMember("skillsPosition", Vec3ToJson(skillsPosition, alloc), alloc);

			// Skill appearance states
			out.AddMember("skillReadyTint", Vec3ToJson(skillReadyTint, alloc), alloc);
			out.AddMember("skillReadyAlpha", skillReadyAlpha, alloc);
			out.AddMember("skillCooldownTint", Vec3ToJson(skillCooldownTint, alloc), alloc);
			out.AddMember("skillCooldownAlpha", skillCooldownAlpha, alloc);
			out.AddMember("skillLowHealthTint", Vec3ToJson(skillLowHealthTint, alloc), alloc);
			out.AddMember("skillLowHealthAlpha", skillLowHealthAlpha, alloc);
			out.AddMember("skillFallbackColor", Vec3ToJson(skillFallbackColor, alloc), alloc);
			out.AddMember("skillFallbackAlpha", skillFallbackAlpha, alloc);
			out.AddMember("skillCooldownOverlayColor", Vec3ToJson(skillCooldownOverlayColor, alloc), alloc);
			out.AddMember("skillCooldownOverlayAlpha", skillCooldownOverlayAlpha, alloc);
			out.AddMember("skillFlashDuration", skillFlashDuration, alloc);
			out.AddMember("skillFlashColor", Vec3ToJson(skillFlashColor, alloc), alloc);
			out.AddMember("skillFlashGlowSize", skillFlashGlowSize, alloc);
			out.AddMember("skillKeybindTextScale", skillKeybindTextScale, alloc);
			out.AddMember("skillKeybindOffsetY", skillKeybindOffsetY, alloc);
			out.AddMember("skillKeybindColor", Vec3ToJson(skillKeybindColor, alloc), alloc);
			out.AddMember("skillKeybindAlphaReady", skillKeybindAlphaReady, alloc);
			out.AddMember("skillKeybindAlphaNotReady", skillKeybindAlphaNotReady, alloc);

			out.AddMember("showCrosshair", showCrosshair, alloc);
			out.AddMember("crosshairColor", Vec3ToJson(crosshairColor, alloc), alloc);
			out.AddMember("crosshairSize", crosshairSize, alloc);
			out.AddMember("crosshairThickness", crosshairThickness, alloc);
			out.AddMember("crosshairStyle", crosshairStyle, alloc);
			out.AddMember("crosshairGap", crosshairGap, alloc);

			// Health and Mana
			out.AddMember("currentHealth", currentHealth, alloc);
			out.AddMember("maxHealth", maxHealth, alloc);
			out.AddMember("healthRegenRate", healthRegenRate, alloc);
			out.AddMember("healthRegenDelay", healthRegenDelay, alloc);
			out.AddMember("showManaBar", showManaBar, alloc);
			out.AddMember("currentMana", currentMana, alloc);
			out.AddMember("maxMana", maxMana, alloc);
			out.AddMember("manaRegenRate", manaRegenRate, alloc);
			out.AddMember("manaRegenDelay", manaRegenDelay, alloc);
			out.AddMember("manaBarColor", Vec3ToJson(manaBarColor, alloc), alloc);
			out.AddMember("manaBarBgColor", Vec3ToJson(manaBarBgColor, alloc), alloc);
			out.AddMember("manaBarWidth", manaBarWidth, alloc);
			out.AddMember("manaBarHeight", manaBarHeight, alloc);
			out.AddMember("manaBarPosition", Vec3ToJson(manaBarPosition, alloc), alloc);

			// Skill slots
			rapidjson::Value skillsArray(rapidjson::kArrayType);
			for (const auto& skill : skills) {
				rapidjson::Value skillObj(rapidjson::kObjectType);
				skillObj.AddMember("maxCooldown", skill.maxCooldown, alloc);
				skillObj.AddMember("manaCost", skill.manaCost, alloc);
				skillObj.AddMember("slotColor", Vec3ToJson(skill.slotColor, alloc), alloc);
				skillObj.AddMember("readyColor", Vec3ToJson(skill.readyColor, alloc), alloc);
				skillObj.AddMember("cooldownColor", Vec3ToJson(skill.cooldownColor, alloc), alloc);
				skillObj.AddMember("cooldownOverlayColor", Vec3ToJson(skill.cooldownOverlayColor, alloc), alloc);
				// Serialize icon texture path and skill info
				rapidjson::Value iconPathVal(skill.iconTexturePath.c_str(), alloc);
				skillObj.AddMember("iconTexturePath", iconPathVal, alloc);
				rapidjson::Value skillNameVal(skill.skillName.c_str(), alloc);
				skillObj.AddMember("skillName", skillNameVal, alloc);
				rapidjson::Value keyBindingVal(skill.keyBinding.c_str(), alloc);
				skillObj.AddMember("keyBinding", keyBindingVal, alloc);
				rapidjson::Value descriptionVal(skill.description.c_str(), alloc);
				skillObj.AddMember("description", descriptionVal, alloc);
				skillsArray.PushBack(skillObj, alloc);
			}
			out.AddMember("skillSlots", skillsArray, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showHealthbar") && in["showHealthbar"].IsBool())
				showHealthbar = in["showHealthbar"].GetBool();
			if (in.HasMember("healthbarColor") && in["healthbarColor"].IsArray())
				healthbarColor = JsonToVec3(in["healthbarColor"]);
			if (in.HasMember("healthbarBgColor") && in["healthbarBgColor"].IsArray())
				healthbarBgColor = JsonToVec3(in["healthbarBgColor"]);
			if (in.HasMember("healthbarWidth") && in["healthbarWidth"].IsNumber())
				healthbarWidth = in["healthbarWidth"].GetFloat();
			if (in.HasMember("healthbarHeight") && in["healthbarHeight"].IsNumber())
				healthbarHeight = in["healthbarHeight"].GetFloat();
			if (in.HasMember("healthbarPosition") && in["healthbarPosition"].IsArray())
				healthbarPosition = JsonToVec3(in["healthbarPosition"]);
			if (in.HasMember("healthbarBgTexture") && in["healthbarBgTexture"].IsString())
				healthbarBgTexture = in["healthbarBgTexture"].GetString();
			if (in.HasMember("healthbarFillTexture") && in["healthbarFillTexture"].IsString())
				healthbarFillTexture = in["healthbarFillTexture"].GetString();
			if (in.HasMember("healthbarFrameTexture") && in["healthbarFrameTexture"].IsString())
				healthbarFrameTexture = in["healthbarFrameTexture"].GetString();

			if (in.HasMember("showBookCounter") && in["showBookCounter"].IsBool())
				showBookCounter = in["showBookCounter"].GetBool();
			if (in.HasMember("booksCollected") && in["booksCollected"].IsInt())
				booksCollected = in["booksCollected"].GetInt();
			if (in.HasMember("totalBooks") && in["totalBooks"].IsInt())
				totalBooks = in["totalBooks"].GetInt();
			if (in.HasMember("bookCounterPosition") && in["bookCounterPosition"].IsArray())
				bookCounterPosition = JsonToVec3(in["bookCounterPosition"]);

			if (in.HasMember("showSkills") && in["showSkills"].IsBool())
				showSkills = in["showSkills"].GetBool();
			if (in.HasMember("skillSlotSize") && in["skillSlotSize"].IsNumber())
				skillSlotSize = in["skillSlotSize"].GetFloat();
			if (in.HasMember("skillSlotSpacing") && in["skillSlotSpacing"].IsNumber())
				skillSlotSpacing = in["skillSlotSpacing"].GetFloat();
			if (in.HasMember("skillsPosition") && in["skillsPosition"].IsArray())
				skillsPosition = JsonToVec3(in["skillsPosition"]);

			// Skill appearance states
			if (in.HasMember("skillReadyTint") && in["skillReadyTint"].IsArray())
				skillReadyTint = JsonToVec3(in["skillReadyTint"]);
			if (in.HasMember("skillReadyAlpha") && in["skillReadyAlpha"].IsNumber())
				skillReadyAlpha = in["skillReadyAlpha"].GetFloat();
			if (in.HasMember("skillCooldownTint") && in["skillCooldownTint"].IsArray())
				skillCooldownTint = JsonToVec3(in["skillCooldownTint"]);
			if (in.HasMember("skillCooldownAlpha") && in["skillCooldownAlpha"].IsNumber())
				skillCooldownAlpha = in["skillCooldownAlpha"].GetFloat();
			if (in.HasMember("skillLowHealthTint") && in["skillLowHealthTint"].IsArray())
				skillLowHealthTint = JsonToVec3(in["skillLowHealthTint"]);
			if (in.HasMember("skillLowHealthAlpha") && in["skillLowHealthAlpha"].IsNumber())
				skillLowHealthAlpha = in["skillLowHealthAlpha"].GetFloat();
			if (in.HasMember("skillFallbackColor") && in["skillFallbackColor"].IsArray())
				skillFallbackColor = JsonToVec3(in["skillFallbackColor"]);
			if (in.HasMember("skillFallbackAlpha") && in["skillFallbackAlpha"].IsNumber())
				skillFallbackAlpha = in["skillFallbackAlpha"].GetFloat();
			if (in.HasMember("skillCooldownOverlayColor") && in["skillCooldownOverlayColor"].IsArray())
				skillCooldownOverlayColor = JsonToVec3(in["skillCooldownOverlayColor"]);
			if (in.HasMember("skillCooldownOverlayAlpha") && in["skillCooldownOverlayAlpha"].IsNumber())
				skillCooldownOverlayAlpha = in["skillCooldownOverlayAlpha"].GetFloat();
			if (in.HasMember("skillFlashDuration") && in["skillFlashDuration"].IsNumber())
				skillFlashDuration = in["skillFlashDuration"].GetFloat();
			if (in.HasMember("skillFlashColor") && in["skillFlashColor"].IsArray())
				skillFlashColor = JsonToVec3(in["skillFlashColor"]);
			if (in.HasMember("skillFlashGlowSize") && in["skillFlashGlowSize"].IsNumber())
				skillFlashGlowSize = in["skillFlashGlowSize"].GetFloat();
			if (in.HasMember("skillKeybindTextScale") && in["skillKeybindTextScale"].IsNumber())
				skillKeybindTextScale = in["skillKeybindTextScale"].GetFloat();
			if (in.HasMember("skillKeybindOffsetY") && in["skillKeybindOffsetY"].IsNumber())
				skillKeybindOffsetY = in["skillKeybindOffsetY"].GetFloat();
			if (in.HasMember("skillKeybindColor") && in["skillKeybindColor"].IsArray())
				skillKeybindColor = JsonToVec3(in["skillKeybindColor"]);
			if (in.HasMember("skillKeybindAlphaReady") && in["skillKeybindAlphaReady"].IsNumber())
				skillKeybindAlphaReady = in["skillKeybindAlphaReady"].GetFloat();
			if (in.HasMember("skillKeybindAlphaNotReady") && in["skillKeybindAlphaNotReady"].IsNumber())
				skillKeybindAlphaNotReady = in["skillKeybindAlphaNotReady"].GetFloat();

			if (in.HasMember("showCrosshair") && in["showCrosshair"].IsBool())
				showCrosshair = in["showCrosshair"].GetBool();
			if (in.HasMember("crosshairColor") && in["crosshairColor"].IsArray())
				crosshairColor = JsonToVec3(in["crosshairColor"]);
			if (in.HasMember("crosshairSize") && in["crosshairSize"].IsNumber())
				crosshairSize = in["crosshairSize"].GetFloat();
			if (in.HasMember("crosshairThickness") && in["crosshairThickness"].IsNumber())
				crosshairThickness = in["crosshairThickness"].GetFloat();
			if (in.HasMember("crosshairStyle") && in["crosshairStyle"].IsInt())
				crosshairStyle = in["crosshairStyle"].GetInt();
			if (in.HasMember("crosshairGap") && in["crosshairGap"].IsNumber())
				crosshairGap = in["crosshairGap"].GetFloat();

			// Health and Mana
			if (in.HasMember("currentHealth") && in["currentHealth"].IsNumber())
				currentHealth = in["currentHealth"].GetFloat();
			if (in.HasMember("maxHealth") && in["maxHealth"].IsNumber())
				maxHealth = in["maxHealth"].GetFloat();
			if (in.HasMember("healthRegenRate") && in["healthRegenRate"].IsNumber())
				healthRegenRate = in["healthRegenRate"].GetFloat();
			if (in.HasMember("healthRegenDelay") && in["healthRegenDelay"].IsNumber())
				healthRegenDelay = in["healthRegenDelay"].GetFloat();
			if (in.HasMember("showManaBar") && in["showManaBar"].IsBool())
				showManaBar = in["showManaBar"].GetBool();
			if (in.HasMember("currentMana") && in["currentMana"].IsNumber())
				currentMana = in["currentMana"].GetFloat();
			if (in.HasMember("maxMana") && in["maxMana"].IsNumber())
				maxMana = in["maxMana"].GetFloat();
			if (in.HasMember("manaRegenRate") && in["manaRegenRate"].IsNumber())
				manaRegenRate = in["manaRegenRate"].GetFloat();
			if (in.HasMember("manaRegenDelay") && in["manaRegenDelay"].IsNumber())
				manaRegenDelay = in["manaRegenDelay"].GetFloat();
			if (in.HasMember("manaBarColor") && in["manaBarColor"].IsArray())
				manaBarColor = JsonToVec3(in["manaBarColor"]);
			if (in.HasMember("manaBarBgColor") && in["manaBarBgColor"].IsArray())
				manaBarBgColor = JsonToVec3(in["manaBarBgColor"]);
			if (in.HasMember("manaBarWidth") && in["manaBarWidth"].IsNumber())
				manaBarWidth = in["manaBarWidth"].GetFloat();
			if (in.HasMember("manaBarHeight") && in["manaBarHeight"].IsNumber())
				manaBarHeight = in["manaBarHeight"].GetFloat();
			if (in.HasMember("manaBarPosition") && in["manaBarPosition"].IsArray())
				manaBarPosition = JsonToVec3(in["manaBarPosition"]);

			// Skill slots
			if (in.HasMember("skillSlots") && in["skillSlots"].IsArray()) {
				const auto& skillsArray = in["skillSlots"];
				size_t count = std::min(skillsArray.Size(), static_cast<unsigned int>(skills.size()));
				for (size_t i = 0; i < count; ++i) {
					const auto& skillObj = skillsArray[i];
					if (skillObj.HasMember("maxCooldown") && skillObj["maxCooldown"].IsNumber())
						skills[i].maxCooldown = skillObj["maxCooldown"].GetFloat();
					if (skillObj.HasMember("manaCost") && skillObj["manaCost"].IsNumber())
						skills[i].manaCost = skillObj["manaCost"].GetFloat();
					if (skillObj.HasMember("slotColor") && skillObj["slotColor"].IsArray())
						skills[i].slotColor = JsonToVec3(skillObj["slotColor"]);
					if (skillObj.HasMember("readyColor") && skillObj["readyColor"].IsArray())
						skills[i].readyColor = JsonToVec3(skillObj["readyColor"]);
					if (skillObj.HasMember("cooldownColor") && skillObj["cooldownColor"].IsArray())
						skills[i].cooldownColor = JsonToVec3(skillObj["cooldownColor"]);
					if (skillObj.HasMember("cooldownOverlayColor") && skillObj["cooldownOverlayColor"].IsArray())
						skills[i].cooldownOverlayColor = JsonToVec3(skillObj["cooldownOverlayColor"]);
					// Deserialize icon texture path and skill info
					if (skillObj.HasMember("iconTexturePath") && skillObj["iconTexturePath"].IsString())
						skills[i].iconTexturePath = skillObj["iconTexturePath"].GetString();
					if (skillObj.HasMember("skillName") && skillObj["skillName"].IsString())
						skills[i].skillName = skillObj["skillName"].GetString();
					if (skillObj.HasMember("keyBinding") && skillObj["keyBinding"].IsString())
						skills[i].keyBinding = skillObj["keyBinding"].GetString();
					if (skillObj.HasMember("description") && skillObj["description"].IsString())
						skills[i].description = skillObj["description"].GetString();

					// Reset runtime values (these should not persist between sessions)
					skills[i].currentCooldown = 0.0f;
					skills[i].isOnCooldown = false;
				}
			}

			// Reset component runtime timers
			healthRegenTimer = 0.0f;
			manaRegenTimer = 0.0f;
		}

		XPROPERTY_DEF(
			"UIComponent", UIComponent,
			// Healthbar settings
			xproperty::obj_member<"showHealthbar", &UIComponent::showHealthbar>,
			xproperty::obj_member<"healthbarColor", &UIComponent::healthbarColor>,
			xproperty::obj_member<"healthbarBgColor", &UIComponent::healthbarBgColor>,
			xproperty::obj_member<"healthbarWidth", &UIComponent::healthbarWidth>,
			xproperty::obj_member<"healthbarHeight", &UIComponent::healthbarHeight>,
			xproperty::obj_member<"healthbarPosition", &UIComponent::healthbarPosition>,
			xproperty::obj_member<"healthbarBgTexture", &UIComponent::healthbarBgTexture>,
			xproperty::obj_member<"healthbarFillTexture", &UIComponent::healthbarFillTexture>,
			xproperty::obj_member<"healthbarFrameTexture", &UIComponent::healthbarFrameTexture>,

			// Health system
			xproperty::obj_member<"currentHealth", &UIComponent::currentHealth>,
			xproperty::obj_member<"maxHealth", &UIComponent::maxHealth>,
			xproperty::obj_member<"healthRegenRate", &UIComponent::healthRegenRate>,
			xproperty::obj_member<"healthRegenDelay", &UIComponent::healthRegenDelay>,

			// Mana bar settings
			xproperty::obj_member<"showManaBar", &UIComponent::showManaBar>,
			xproperty::obj_member<"currentMana", &UIComponent::currentMana>,
			xproperty::obj_member<"maxMana", &UIComponent::maxMana>,
			xproperty::obj_member<"manaRegenRate", &UIComponent::manaRegenRate>,
			xproperty::obj_member<"manaRegenDelay", &UIComponent::manaRegenDelay>,
			xproperty::obj_member<"manaBarColor", &UIComponent::manaBarColor>,

			// Skills UI settings
			xproperty::obj_member<"showSkills", &UIComponent::showSkills>,
			xproperty::obj_member<"skillSlotSize", &UIComponent::skillSlotSize>,
			xproperty::obj_member<"skillSlotSpacing", &UIComponent::skillSlotSpacing>,
			xproperty::obj_member<"skillsPosition", &UIComponent::skillsPosition>,

			// Skill appearance states
			xproperty::obj_member<"skillReadyTint", &UIComponent::skillReadyTint>,
			xproperty::obj_member<"skillReadyAlpha", &UIComponent::skillReadyAlpha>,
			xproperty::obj_member<"skillCooldownTint", &UIComponent::skillCooldownTint>,
			xproperty::obj_member<"skillCooldownAlpha", &UIComponent::skillCooldownAlpha>,
			xproperty::obj_member<"skillLowHealthTint", &UIComponent::skillLowHealthTint>,
			xproperty::obj_member<"skillLowHealthAlpha", &UIComponent::skillLowHealthAlpha>,
			xproperty::obj_member<"skillFallbackColor", &UIComponent::skillFallbackColor>,
			xproperty::obj_member<"skillFallbackAlpha", &UIComponent::skillFallbackAlpha>,
			xproperty::obj_member<"skillCooldownOverlayColor", &UIComponent::skillCooldownOverlayColor>,
			xproperty::obj_member<"skillCooldownOverlayAlpha", &UIComponent::skillCooldownOverlayAlpha>,
			xproperty::obj_member<"skillFlashDuration", &UIComponent::skillFlashDuration>,
			xproperty::obj_member<"skillFlashColor", &UIComponent::skillFlashColor>,
			xproperty::obj_member<"skillFlashGlowSize", &UIComponent::skillFlashGlowSize>,
			xproperty::obj_member<"skillKeybindTextScale", &UIComponent::skillKeybindTextScale>,
			xproperty::obj_member<"skillKeybindOffsetY", &UIComponent::skillKeybindOffsetY>,
			xproperty::obj_member<"skillKeybindColor", &UIComponent::skillKeybindColor>,
			xproperty::obj_member<"skillKeybindAlphaReady", &UIComponent::skillKeybindAlphaReady>,
			xproperty::obj_member<"skillKeybindAlphaNotReady", &UIComponent::skillKeybindAlphaNotReady>,

			// Crosshair settings
			xproperty::obj_member<"showCrosshair", &UIComponent::showCrosshair>,
			xproperty::obj_member<"crosshairSize", &UIComponent::crosshairSize>,
			xproperty::obj_member<"crosshairStyle", &UIComponent::crosshairStyle>
		)
	};

	/*!***********************************************************************
	\brief
		UI Healthbar Component - displays player health with optional textures.
		Separate from other UI elements for better organization.
	*************************************************************************/
	struct UIHealthbarComponent
	{
		// Healthbar settings
		bool showHealthbar = true;
		Ermine::Vec3 healthbarColor = { 0.85f, 0.15f, 0.15f };      // Red for health (normal)
		Ermine::Vec3 healthbarLowColor = { 0.85f, 0.45f, 0.15f };   // Color when health < 50%
		Ermine::Vec3 healthbarCriticalColor = { 0.75f, 0.20f, 0.10f }; // Color when health < 25%
		Ermine::Vec3 healthbarBgColor = { 0.2f, 0.2f, 0.2f };       // Dark gray
		float healthbarWidth = 0.30f;  // Percentage of screen width
		float healthbarHeight = 0.03f; // Percentage of screen height
		Ermine::Vec3 healthbarPosition = { 0.02f, 0.92f, 0.0f };  // Top-left corner
		std::string healthbarBgTexture = "";      // Background texture path (optional)
		std::string healthbarFillTexture = "";    // Fill texture path (optional)
		std::string healthbarFrameTexture = "";   // Frame texture path (optional)

		// Shine effect (only visible without fill texture)
		Ermine::Vec3 healthbarShineColor = { 1.0f, 1.0f, 1.0f };    // Shine overlay color
		float healthbarShineAlpha = 0.3f;                           // Shine overlay alpha

		// Health system (Life Essence)
		float currentHealth = 50.0f;
		float maxHealth = 50.0f;
		float healthRegenRate = 5.0f;          // Health per second when regenerating
		float healthRegenDelay = 3.0f;         // Delay after damage before regen starts
		float healthRegenTimer = 0.0f;         // Internal timer (don't serialize)

		float GetHealth() const { return currentHealth; }
		void SetHealth(float value) { currentHealth = std::clamp(value, 0.0f, maxHealth); }

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showHealthbar", showHealthbar, alloc);
			out.AddMember("healthbarColor", Vec3ToJson(healthbarColor, alloc), alloc);
			out.AddMember("healthbarLowColor", Vec3ToJson(healthbarLowColor, alloc), alloc);
			out.AddMember("healthbarCriticalColor", Vec3ToJson(healthbarCriticalColor, alloc), alloc);
			out.AddMember("healthbarBgColor", Vec3ToJson(healthbarBgColor, alloc), alloc);
			out.AddMember("healthbarWidth", healthbarWidth, alloc);
			out.AddMember("healthbarHeight", healthbarHeight, alloc);
			out.AddMember("healthbarPosition", Vec3ToJson(healthbarPosition, alloc), alloc);
			rapidjson::Value bgTexVal(healthbarBgTexture.c_str(), alloc);
			out.AddMember("healthbarBgTexture", bgTexVal, alloc);
			rapidjson::Value fillTexVal(healthbarFillTexture.c_str(), alloc);
			out.AddMember("healthbarFillTexture", fillTexVal, alloc);
			rapidjson::Value frameTexVal(healthbarFrameTexture.c_str(), alloc);
			out.AddMember("healthbarFrameTexture", frameTexVal, alloc);
			out.AddMember("healthbarShineColor", Vec3ToJson(healthbarShineColor, alloc), alloc);
			out.AddMember("healthbarShineAlpha", healthbarShineAlpha, alloc);
			out.AddMember("currentHealth", currentHealth, alloc);
			out.AddMember("maxHealth", maxHealth, alloc);
			out.AddMember("healthRegenRate", healthRegenRate, alloc);
			out.AddMember("healthRegenDelay", healthRegenDelay, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showHealthbar") && in["showHealthbar"].IsBool())
				showHealthbar = in["showHealthbar"].GetBool();
			if (in.HasMember("healthbarColor") && in["healthbarColor"].IsArray())
				healthbarColor = JsonToVec3(in["healthbarColor"]);
			if (in.HasMember("healthbarLowColor") && in["healthbarLowColor"].IsArray())
				healthbarLowColor = JsonToVec3(in["healthbarLowColor"]);
			if (in.HasMember("healthbarCriticalColor") && in["healthbarCriticalColor"].IsArray())
				healthbarCriticalColor = JsonToVec3(in["healthbarCriticalColor"]);
			if (in.HasMember("healthbarBgColor") && in["healthbarBgColor"].IsArray())
				healthbarBgColor = JsonToVec3(in["healthbarBgColor"]);
			if (in.HasMember("healthbarWidth") && in["healthbarWidth"].IsNumber())
				healthbarWidth = in["healthbarWidth"].GetFloat();
			if (in.HasMember("healthbarHeight") && in["healthbarHeight"].IsNumber())
				healthbarHeight = in["healthbarHeight"].GetFloat();
			if (in.HasMember("healthbarPosition") && in["healthbarPosition"].IsArray())
				healthbarPosition = JsonToVec3(in["healthbarPosition"]);
			if (in.HasMember("healthbarBgTexture") && in["healthbarBgTexture"].IsString())
				healthbarBgTexture = in["healthbarBgTexture"].GetString();
			if (in.HasMember("healthbarFillTexture") && in["healthbarFillTexture"].IsString())
				healthbarFillTexture = in["healthbarFillTexture"].GetString();
			if (in.HasMember("healthbarFrameTexture") && in["healthbarFrameTexture"].IsString())
				healthbarFrameTexture = in["healthbarFrameTexture"].GetString();
			if (in.HasMember("healthbarShineColor") && in["healthbarShineColor"].IsArray())
				healthbarShineColor = JsonToVec3(in["healthbarShineColor"]);
			if (in.HasMember("healthbarShineAlpha") && in["healthbarShineAlpha"].IsNumber())
				healthbarShineAlpha = in["healthbarShineAlpha"].GetFloat();
			if (in.HasMember("currentHealth") && in["currentHealth"].IsNumber())
				currentHealth = in["currentHealth"].GetFloat();
			if (in.HasMember("maxHealth") && in["maxHealth"].IsNumber())
				maxHealth = in["maxHealth"].GetFloat();
			if (in.HasMember("healthRegenRate") && in["healthRegenRate"].IsNumber())
				healthRegenRate = in["healthRegenRate"].GetFloat();
			if (in.HasMember("healthRegenDelay") && in["healthRegenDelay"].IsNumber())
				healthRegenDelay = in["healthRegenDelay"].GetFloat();
			healthRegenTimer = 0.0f;
		}

		XPROPERTY_DEF(
			"UIHealthbarComponent", UIHealthbarComponent,
			xproperty::obj_member<"showHealthbar", &UIHealthbarComponent::showHealthbar>,
			xproperty::obj_member<"healthbarColor", &UIHealthbarComponent::healthbarColor>,
			xproperty::obj_member<"healthbarLowColor", &UIHealthbarComponent::healthbarLowColor>,
			xproperty::obj_member<"healthbarCriticalColor", &UIHealthbarComponent::healthbarCriticalColor>,
			xproperty::obj_member<"healthbarBgColor", &UIHealthbarComponent::healthbarBgColor>,
			xproperty::obj_member<"healthbarWidth", &UIHealthbarComponent::healthbarWidth>,
			xproperty::obj_member<"healthbarHeight", &UIHealthbarComponent::healthbarHeight>,
			xproperty::obj_member<"healthbarPosition", &UIHealthbarComponent::healthbarPosition>,
			xproperty::obj_member<"healthbarBgTexture", &UIHealthbarComponent::healthbarBgTexture>,
			xproperty::obj_member<"healthbarFillTexture", &UIHealthbarComponent::healthbarFillTexture>,
			xproperty::obj_member<"healthbarFrameTexture", &UIHealthbarComponent::healthbarFrameTexture>,
			xproperty::obj_member<"healthbarShineColor", &UIHealthbarComponent::healthbarShineColor>,
			xproperty::obj_member<"healthbarShineAlpha", &UIHealthbarComponent::healthbarShineAlpha>,
			xproperty::obj_member<"currentHealth", &UIHealthbarComponent::currentHealth>,
			xproperty::obj_member<"maxHealth", &UIHealthbarComponent::maxHealth>,
			xproperty::obj_member<"healthRegenRate", &UIHealthbarComponent::healthRegenRate>,
			xproperty::obj_member<"healthRegenDelay", &UIHealthbarComponent::healthRegenDelay>
		)
	};

	/*!***********************************************************************
	\brief
		UI Crosshair Component - displays crosshair for aiming.
	*************************************************************************/
	struct UICrosshairComponent
	{
		bool showCrosshair = true;
		std::string crosshairTexturePath = "../Resources/Textures/UI/crosshair.png";  // Texture path (editable in inspector!)
		Ermine::Vec3 crosshairColor = { 0.95f, 0.95f, 0.95f };  // Bright white
		float crosshairSize = 0.05f;   // Reduced size for better precision
		float crosshairThickness = 0.001f;  // Thinner and sharper
		int crosshairStyle = 0;        // 0 = sniper scope, 1 = dot, 2 = circle
		float crosshairGap = 0.004f;   // Small center gap for precise aiming

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showCrosshair", showCrosshair, alloc);
			rapidjson::Value texPathVal(crosshairTexturePath.c_str(), alloc);
			out.AddMember("crosshairTexturePath", texPathVal, alloc);
			out.AddMember("crosshairColor", Vec3ToJson(crosshairColor, alloc), alloc);
			out.AddMember("crosshairSize", crosshairSize, alloc);
			out.AddMember("crosshairThickness", crosshairThickness, alloc);
			out.AddMember("crosshairStyle", crosshairStyle, alloc);
			out.AddMember("crosshairGap", crosshairGap, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showCrosshair") && in["showCrosshair"].IsBool())
				showCrosshair = in["showCrosshair"].GetBool();
			if (in.HasMember("crosshairTexturePath") && in["crosshairTexturePath"].IsString())
				crosshairTexturePath = in["crosshairTexturePath"].GetString();
			if (in.HasMember("crosshairColor") && in["crosshairColor"].IsArray())
				crosshairColor = JsonToVec3(in["crosshairColor"]);
			if (in.HasMember("crosshairSize") && in["crosshairSize"].IsNumber())
				crosshairSize = in["crosshairSize"].GetFloat();
			if (in.HasMember("crosshairThickness") && in["crosshairThickness"].IsNumber())
				crosshairThickness = in["crosshairThickness"].GetFloat();
			if (in.HasMember("crosshairStyle") && in["crosshairStyle"].IsInt())
				crosshairStyle = in["crosshairStyle"].GetInt();
			if (in.HasMember("crosshairGap") && in["crosshairGap"].IsNumber())
				crosshairGap = in["crosshairGap"].GetFloat();
		}

		XPROPERTY_DEF(
			"UICrosshairComponent", UICrosshairComponent,
			xproperty::obj_member<"showCrosshair", &UICrosshairComponent::showCrosshair>,
			xproperty::obj_member<"crosshairTexturePath", &UICrosshairComponent::crosshairTexturePath>,
			xproperty::obj_member<"crosshairColor", &UICrosshairComponent::crosshairColor>,
			xproperty::obj_member<"crosshairSize", &UICrosshairComponent::crosshairSize>,
			xproperty::obj_member<"crosshairThickness", &UICrosshairComponent::crosshairThickness>,
			xproperty::obj_member<"crosshairStyle", &UICrosshairComponent::crosshairStyle>,
			xproperty::obj_member<"crosshairGap", &UICrosshairComponent::crosshairGap>
		)
	};

	/*!***********************************************************************
	\brief
		UI Skills Component - displays skill slots with cooldowns and keybinds.
	*************************************************************************/
	struct UISkillsComponent
	{
		// Skills UI settings
		bool showSkills = true;
		float skillSlotSize = 0.06f;   // Default size for all slots (can be overridden per-slot)

		// Skill appearance states (default for all slots)
		Ermine::Vec3 skillReadyTint = { 1.0f, 1.0f, 1.0f };
		float skillReadyAlpha = 1.0f;
		Ermine::Vec3 skillCooldownTint = { 0.5f, 0.5f, 0.5f };
		float skillCooldownAlpha = 0.6f;
		Ermine::Vec3 skillLowHealthTint = { 0.7f, 0.7f, 0.7f };
		float skillLowHealthAlpha = 0.7f;
		Ermine::Vec3 skillFallbackColor = { 0.3f, 0.3f, 0.3f };
		float skillFallbackAlpha = 0.5f;

		// Skill cooldown overlay
		Ermine::Vec3 skillCooldownOverlayColor = { 0.0f, 0.0f, 0.0f };
		float skillCooldownOverlayAlpha = 0.7f;

		// Skill activation flash effect
		float skillFlashDuration = 0.2f;
		Ermine::Vec3 skillFlashColor = { 1.0f, 1.0f, 0.8f };
		float skillFlashGlowSize = 0.02f;

		// Skill keybind label
		float skillKeybindTextScale = 0.6f;
		float skillKeybindOffsetY = 0.02f;
		Ermine::Vec3 skillKeybindColor = { 1.0f, 1.0f, 1.0f };
		float skillKeybindAlphaReady = 1.0f;
		float skillKeybindAlphaNotReady = 0.6f;

		// Skill slot data - EACH SLOT HAS ITS OWN POSITION!
		struct SkillSlot
		{
			Ermine::Vec3 position = { 0.5f, 0.1f, 0.0f };  // Individual position for this slot
			float size = 0.0f;  // Size override (0 = use component's skillSlotSize)
			float currentCooldown = 0.0f;
			float maxCooldown = 5.0f;
			float manaCost = 20.0f;
			bool isOnCooldown = false;
			float activationFlashTimer = 0.0f;
			Ermine::Vec3 slotColor = { 0.25f, 0.25f, 0.25f };
			Ermine::Vec3 readyColor = { 0.85f, 0.85f, 0.85f };
			Ermine::Vec3 cooldownColor = { 0.45f, 0.45f, 0.45f };
			Ermine::Vec3 cooldownOverlayColor = { 0.15f, 0.15f, 0.15f };
			std::string iconTexturePath = "";
			std::string selectedIconPath = "";    // Icon when skill is selected/active
			std::string unselectedIconPath = "";  // Icon when skill is not selected
			bool isSelected = false;              // Current selection state
			std::string skillName = "";
			std::string keyBinding = "";
			std::string description = "";
		};
		std::vector<SkillSlot> skills;

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showSkills", showSkills, alloc);
			out.AddMember("skillSlotSize", skillSlotSize, alloc);
			out.AddMember("skillReadyTint", Vec3ToJson(skillReadyTint, alloc), alloc);
			out.AddMember("skillReadyAlpha", skillReadyAlpha, alloc);
			out.AddMember("skillCooldownTint", Vec3ToJson(skillCooldownTint, alloc), alloc);
			out.AddMember("skillCooldownAlpha", skillCooldownAlpha, alloc);
			out.AddMember("skillLowHealthTint", Vec3ToJson(skillLowHealthTint, alloc), alloc);
			out.AddMember("skillLowHealthAlpha", skillLowHealthAlpha, alloc);
			out.AddMember("skillFallbackColor", Vec3ToJson(skillFallbackColor, alloc), alloc);
			out.AddMember("skillFallbackAlpha", skillFallbackAlpha, alloc);
			out.AddMember("skillCooldownOverlayColor", Vec3ToJson(skillCooldownOverlayColor, alloc), alloc);
			out.AddMember("skillCooldownOverlayAlpha", skillCooldownOverlayAlpha, alloc);
			out.AddMember("skillFlashDuration", skillFlashDuration, alloc);
			out.AddMember("skillFlashColor", Vec3ToJson(skillFlashColor, alloc), alloc);
			out.AddMember("skillFlashGlowSize", skillFlashGlowSize, alloc);
			out.AddMember("skillKeybindTextScale", skillKeybindTextScale, alloc);
			out.AddMember("skillKeybindOffsetY", skillKeybindOffsetY, alloc);
			out.AddMember("skillKeybindColor", Vec3ToJson(skillKeybindColor, alloc), alloc);
			out.AddMember("skillKeybindAlphaReady", skillKeybindAlphaReady, alloc);
			out.AddMember("skillKeybindAlphaNotReady", skillKeybindAlphaNotReady, alloc);

			rapidjson::Value skillsArray(rapidjson::kArrayType);
			for (const auto& skill : skills) {
				rapidjson::Value skillObj(rapidjson::kObjectType);
				skillObj.AddMember("position", Vec3ToJson(skill.position, alloc), alloc);
				skillObj.AddMember("size", skill.size, alloc);
				skillObj.AddMember("maxCooldown", skill.maxCooldown, alloc);
				skillObj.AddMember("manaCost", skill.manaCost, alloc);
				skillObj.AddMember("slotColor", Vec3ToJson(skill.slotColor, alloc), alloc);
				skillObj.AddMember("readyColor", Vec3ToJson(skill.readyColor, alloc), alloc);
				skillObj.AddMember("cooldownColor", Vec3ToJson(skill.cooldownColor, alloc), alloc);
				skillObj.AddMember("cooldownOverlayColor", Vec3ToJson(skill.cooldownOverlayColor, alloc), alloc);
				rapidjson::Value iconPathVal(skill.iconTexturePath.c_str(), alloc);
				skillObj.AddMember("iconTexturePath", iconPathVal, alloc);
				rapidjson::Value selectedIconVal(skill.selectedIconPath.c_str(), alloc);
				skillObj.AddMember("selectedIconPath", selectedIconVal, alloc);
				rapidjson::Value unselectedIconVal(skill.unselectedIconPath.c_str(), alloc);
				skillObj.AddMember("unselectedIconPath", unselectedIconVal, alloc);
				skillObj.AddMember("isSelected", skill.isSelected, alloc);
				rapidjson::Value skillNameVal(skill.skillName.c_str(), alloc);
				skillObj.AddMember("skillName", skillNameVal, alloc);
				rapidjson::Value keyBindingVal(skill.keyBinding.c_str(), alloc);
				skillObj.AddMember("keyBinding", keyBindingVal, alloc);
				rapidjson::Value descriptionVal(skill.description.c_str(), alloc);
				skillObj.AddMember("description", descriptionVal, alloc);
				skillsArray.PushBack(skillObj, alloc);
			}
			out.AddMember("skills", skillsArray, alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showSkills") && in["showSkills"].IsBool())
				showSkills = in["showSkills"].GetBool();
			if (in.HasMember("skillSlotSize") && in["skillSlotSize"].IsNumber())
				skillSlotSize = in["skillSlotSize"].GetFloat();
			if (in.HasMember("skillReadyTint") && in["skillReadyTint"].IsArray())
				skillReadyTint = JsonToVec3(in["skillReadyTint"]);
			if (in.HasMember("skillReadyAlpha") && in["skillReadyAlpha"].IsNumber())
				skillReadyAlpha = in["skillReadyAlpha"].GetFloat();
			if (in.HasMember("skillCooldownTint") && in["skillCooldownTint"].IsArray())
				skillCooldownTint = JsonToVec3(in["skillCooldownTint"]);
			if (in.HasMember("skillCooldownAlpha") && in["skillCooldownAlpha"].IsNumber())
				skillCooldownAlpha = in["skillCooldownAlpha"].GetFloat();
			if (in.HasMember("skillLowHealthTint") && in["skillLowHealthTint"].IsArray())
				skillLowHealthTint = JsonToVec3(in["skillLowHealthTint"]);
			if (in.HasMember("skillLowHealthAlpha") && in["skillLowHealthAlpha"].IsNumber())
				skillLowHealthAlpha = in["skillLowHealthAlpha"].GetFloat();
			if (in.HasMember("skillFallbackColor") && in["skillFallbackColor"].IsArray())
				skillFallbackColor = JsonToVec3(in["skillFallbackColor"]);
			if (in.HasMember("skillFallbackAlpha") && in["skillFallbackAlpha"].IsNumber())
				skillFallbackAlpha = in["skillFallbackAlpha"].GetFloat();
			if (in.HasMember("skillCooldownOverlayColor") && in["skillCooldownOverlayColor"].IsArray())
				skillCooldownOverlayColor = JsonToVec3(in["skillCooldownOverlayColor"]);
			if (in.HasMember("skillCooldownOverlayAlpha") && in["skillCooldownOverlayAlpha"].IsNumber())
				skillCooldownOverlayAlpha = in["skillCooldownOverlayAlpha"].GetFloat();
			if (in.HasMember("skillFlashDuration") && in["skillFlashDuration"].IsNumber())
				skillFlashDuration = in["skillFlashDuration"].GetFloat();
			if (in.HasMember("skillFlashColor") && in["skillFlashColor"].IsArray())
				skillFlashColor = JsonToVec3(in["skillFlashColor"]);
			if (in.HasMember("skillFlashGlowSize") && in["skillFlashGlowSize"].IsNumber())
				skillFlashGlowSize = in["skillFlashGlowSize"].GetFloat();
			if (in.HasMember("skillKeybindTextScale") && in["skillKeybindTextScale"].IsNumber())
				skillKeybindTextScale = in["skillKeybindTextScale"].GetFloat();
			if (in.HasMember("skillKeybindOffsetY") && in["skillKeybindOffsetY"].IsNumber())
				skillKeybindOffsetY = in["skillKeybindOffsetY"].GetFloat();
			if (in.HasMember("skillKeybindColor") && in["skillKeybindColor"].IsArray())
				skillKeybindColor = JsonToVec3(in["skillKeybindColor"]);
			if (in.HasMember("skillKeybindAlphaReady") && in["skillKeybindAlphaReady"].IsNumber())
				skillKeybindAlphaReady = in["skillKeybindAlphaReady"].GetFloat();
			if (in.HasMember("skillKeybindAlphaNotReady") && in["skillKeybindAlphaNotReady"].IsNumber())
				skillKeybindAlphaNotReady = in["skillKeybindAlphaNotReady"].GetFloat();

			// Load skills array - supports both old "skillSlots" and new "skills" key for backward compatibility
			const rapidjson::Value* skillsArrayPtr = nullptr;
			if (in.HasMember("skills") && in["skills"].IsArray())
				skillsArrayPtr = &in["skills"];
			else if (in.HasMember("skillSlots") && in["skillSlots"].IsArray())
				skillsArrayPtr = &in["skillSlots"];

			if (skillsArrayPtr) {
				const auto& skillsArray = *skillsArrayPtr;
				skills.clear();
				skills.reserve(skillsArray.Size());
				for (rapidjson::SizeType i = 0; i < skillsArray.Size(); ++i) {
					const auto& skillObj = skillsArray[i];
					SkillSlot slot;
					if (skillObj.HasMember("position") && skillObj["position"].IsArray())
						slot.position = JsonToVec3(skillObj["position"]);
					if (skillObj.HasMember("size") && skillObj["size"].IsNumber())
						slot.size = skillObj["size"].GetFloat();
					if (skillObj.HasMember("maxCooldown") && skillObj["maxCooldown"].IsNumber())
						slot.maxCooldown = skillObj["maxCooldown"].GetFloat();
					if (skillObj.HasMember("manaCost") && skillObj["manaCost"].IsNumber())
						slot.manaCost = skillObj["manaCost"].GetFloat();
					if (skillObj.HasMember("slotColor") && skillObj["slotColor"].IsArray())
						slot.slotColor = JsonToVec3(skillObj["slotColor"]);
					if (skillObj.HasMember("readyColor") && skillObj["readyColor"].IsArray())
						slot.readyColor = JsonToVec3(skillObj["readyColor"]);
					if (skillObj.HasMember("cooldownColor") && skillObj["cooldownColor"].IsArray())
						slot.cooldownColor = JsonToVec3(skillObj["cooldownColor"]);
					if (skillObj.HasMember("cooldownOverlayColor") && skillObj["cooldownOverlayColor"].IsArray())
						slot.cooldownOverlayColor = JsonToVec3(skillObj["cooldownOverlayColor"]);
					if (skillObj.HasMember("iconTexturePath") && skillObj["iconTexturePath"].IsString())
						slot.iconTexturePath = skillObj["iconTexturePath"].GetString();
					if (skillObj.HasMember("selectedIconPath") && skillObj["selectedIconPath"].IsString())
						slot.selectedIconPath = skillObj["selectedIconPath"].GetString();
					if (skillObj.HasMember("unselectedIconPath") && skillObj["unselectedIconPath"].IsString())
						slot.unselectedIconPath = skillObj["unselectedIconPath"].GetString();
					if (skillObj.HasMember("isSelected") && skillObj["isSelected"].IsBool())
						slot.isSelected = skillObj["isSelected"].GetBool();
					if (skillObj.HasMember("skillName") && skillObj["skillName"].IsString())
						slot.skillName = skillObj["skillName"].GetString();
					if (skillObj.HasMember("keyBinding") && skillObj["keyBinding"].IsString())
						slot.keyBinding = skillObj["keyBinding"].GetString();
					if (skillObj.HasMember("description") && skillObj["description"].IsString())
						slot.description = skillObj["description"].GetString();
					slot.currentCooldown = 0.0f;
					slot.isOnCooldown = false;
					skills.push_back(slot);
				}
			}
		}

		XPROPERTY_DEF(
			"UISkillsComponent", UISkillsComponent,
			xproperty::obj_member<"showSkills", &UISkillsComponent::showSkills>,
			xproperty::obj_member<"skillSlotSize", &UISkillsComponent::skillSlotSize>
		)
	};

	/*!***********************************************************************
	\brief
		UI Mana Bar Component - displays mana/energy with regeneration.
	*************************************************************************/
	struct UIManaBarComponent
	{
		bool showManaBar = false;
		float currentMana = 100.0f;
		float maxMana = 100.0f;
		float manaRegenRate = 10.0f;
		float manaRegenDelay = 2.0f;
		float manaRegenTimer = 0.0f;
		Ermine::Vec3 manaBarColor = { 0.0f, 0.5f, 1.0f };
		Ermine::Vec3 manaBarBgColor = { 0.2f, 0.2f, 0.3f };
		float manaBarWidth = 0.3f;
		float manaBarHeight = 0.03f;
		Ermine::Vec3 manaBarPosition = { 0.1f, 0.85f, 0.0f };

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showManaBar", showManaBar, alloc);
			out.AddMember("currentMana", currentMana, alloc);
			out.AddMember("maxMana", maxMana, alloc);
			out.AddMember("manaRegenRate", manaRegenRate, alloc);
			out.AddMember("manaRegenDelay", manaRegenDelay, alloc);
			out.AddMember("manaBarColor", Vec3ToJson(manaBarColor, alloc), alloc);
			out.AddMember("manaBarBgColor", Vec3ToJson(manaBarBgColor, alloc), alloc);
			out.AddMember("manaBarWidth", manaBarWidth, alloc);
			out.AddMember("manaBarHeight", manaBarHeight, alloc);
			out.AddMember("manaBarPosition", Vec3ToJson(manaBarPosition, alloc), alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showManaBar") && in["showManaBar"].IsBool())
				showManaBar = in["showManaBar"].GetBool();
			if (in.HasMember("currentMana") && in["currentMana"].IsNumber())
				currentMana = in["currentMana"].GetFloat();
			if (in.HasMember("maxMana") && in["maxMana"].IsNumber())
				maxMana = in["maxMana"].GetFloat();
			if (in.HasMember("manaRegenRate") && in["manaRegenRate"].IsNumber())
				manaRegenRate = in["manaRegenRate"].GetFloat();
			if (in.HasMember("manaRegenDelay") && in["manaRegenDelay"].IsNumber())
				manaRegenDelay = in["manaRegenDelay"].GetFloat();
			if (in.HasMember("manaBarColor") && in["manaBarColor"].IsArray())
				manaBarColor = JsonToVec3(in["manaBarColor"]);
			if (in.HasMember("manaBarBgColor") && in["manaBarBgColor"].IsArray())
				manaBarBgColor = JsonToVec3(in["manaBarBgColor"]);
			if (in.HasMember("manaBarWidth") && in["manaBarWidth"].IsNumber())
				manaBarWidth = in["manaBarWidth"].GetFloat();
			if (in.HasMember("manaBarHeight") && in["manaBarHeight"].IsNumber())
				manaBarHeight = in["manaBarHeight"].GetFloat();
			if (in.HasMember("manaBarPosition") && in["manaBarPosition"].IsArray())
				manaBarPosition = JsonToVec3(in["manaBarPosition"]);
			manaRegenTimer = 0.0f;
		}

		XPROPERTY_DEF(
			"UIManaBarComponent", UIManaBarComponent,
			xproperty::obj_member<"showManaBar", &UIManaBarComponent::showManaBar>,
			xproperty::obj_member<"currentMana", &UIManaBarComponent::currentMana>,
			xproperty::obj_member<"maxMana", &UIManaBarComponent::maxMana>,
			xproperty::obj_member<"manaRegenRate", &UIManaBarComponent::manaRegenRate>,
			xproperty::obj_member<"manaRegenDelay", &UIManaBarComponent::manaRegenDelay>,
			xproperty::obj_member<"manaBarColor", &UIManaBarComponent::manaBarColor>
		)
	};

	/*!***********************************************************************
	\brief
		UI Book Counter Component - displays collected books count.
	*************************************************************************/
	struct UIBookCounterComponent
	{
		bool showBookCounter = true;
		int booksCollected = 0;
		int totalBooks = 4;
		Ermine::Vec3 bookCounterPosition = { 0.95f, 0.93f, 0.0f };
		Ermine::Vec3 textColor = { 1.0f, 1.0f, 1.0f };  // Text color (white by default)
		float textScale = 1.2f;                         // Text size scale
		float textAlpha = 1.0f;                         // Text transparency

		// Optional textures for visual polish
		std::string bookIconTexture = "";               // Icon to show next to counter (e.g., book icon)
		float bookIconSize = 0.04f;                     // Icon size (if texture provided)
		float bookIconOffsetX = -0.05f;                 // Icon offset from text
		std::string backgroundTexture = "";             // Background panel texture (optional)
		Ermine::Vec3 backgroundSize = { 0.15f, 0.05f, 0.0f }; // Background panel size

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("showBookCounter", showBookCounter, alloc);
			out.AddMember("booksCollected", booksCollected, alloc);
			out.AddMember("totalBooks", totalBooks, alloc);
			out.AddMember("bookCounterPosition", Vec3ToJson(bookCounterPosition, alloc), alloc);
			out.AddMember("textColor", Vec3ToJson(textColor, alloc), alloc);
			out.AddMember("textScale", textScale, alloc);
			out.AddMember("textAlpha", textAlpha, alloc);

			rapidjson::Value iconTexVal(bookIconTexture.c_str(), alloc);
			out.AddMember("bookIconTexture", iconTexVal, alloc);
			out.AddMember("bookIconSize", bookIconSize, alloc);
			out.AddMember("bookIconOffsetX", bookIconOffsetX, alloc);
			rapidjson::Value bgTexVal(backgroundTexture.c_str(), alloc);
			out.AddMember("backgroundTexture", bgTexVal, alloc);
			out.AddMember("backgroundSize", Vec3ToJson(backgroundSize, alloc), alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("showBookCounter") && in["showBookCounter"].IsBool())
				showBookCounter = in["showBookCounter"].GetBool();
			if (in.HasMember("booksCollected") && in["booksCollected"].IsInt())
				booksCollected = in["booksCollected"].GetInt();
			if (in.HasMember("totalBooks") && in["totalBooks"].IsInt())
				totalBooks = in["totalBooks"].GetInt();
			if (in.HasMember("bookCounterPosition") && in["bookCounterPosition"].IsArray())
				bookCounterPosition = JsonToVec3(in["bookCounterPosition"]);
			if (in.HasMember("textColor") && in["textColor"].IsArray())
				textColor = JsonToVec3(in["textColor"]);
			if (in.HasMember("textScale") && in["textScale"].IsNumber())
				textScale = in["textScale"].GetFloat();
			if (in.HasMember("textAlpha") && in["textAlpha"].IsNumber())
				textAlpha = in["textAlpha"].GetFloat();
			if (in.HasMember("bookIconTexture") && in["bookIconTexture"].IsString())
				bookIconTexture = in["bookIconTexture"].GetString();
			if (in.HasMember("bookIconSize") && in["bookIconSize"].IsNumber())
				bookIconSize = in["bookIconSize"].GetFloat();
			if (in.HasMember("bookIconOffsetX") && in["bookIconOffsetX"].IsNumber())
				bookIconOffsetX = in["bookIconOffsetX"].GetFloat();
			if (in.HasMember("backgroundTexture") && in["backgroundTexture"].IsString())
				backgroundTexture = in["backgroundTexture"].GetString();
			if (in.HasMember("backgroundSize") && in["backgroundSize"].IsArray())
				backgroundSize = JsonToVec3(in["backgroundSize"]);
		}

		XPROPERTY_DEF(
			"UIBookCounterComponent", UIBookCounterComponent,
			xproperty::obj_member<"showBookCounter", &UIBookCounterComponent::showBookCounter>,
			xproperty::obj_member<"booksCollected", &UIBookCounterComponent::booksCollected>,
			xproperty::obj_member<"totalBooks", &UIBookCounterComponent::totalBooks>,
			xproperty::obj_member<"bookCounterPosition", &UIBookCounterComponent::bookCounterPosition>,
			xproperty::obj_member<"textColor", &UIBookCounterComponent::textColor>,
			xproperty::obj_member<"textScale", &UIBookCounterComponent::textScale>,
			xproperty::obj_member<"textAlpha", &UIBookCounterComponent::textAlpha>,
			xproperty::obj_member<"bookIconTexture", &UIBookCounterComponent::bookIconTexture>,
			xproperty::obj_member<"bookIconSize", &UIBookCounterComponent::bookIconSize>,
			xproperty::obj_member<"backgroundTexture", &UIBookCounterComponent::backgroundTexture>
		)
	};

	/*!***********************************************************************
	\brief
		UI Image Component for rendering fullscreen or positioned images.
		Used for menus, cutscenes, splash screens, and UI backgrounds.
	*************************************************************************/
	struct UIImageComponent
	{
		std::string imagePath = "";           ///< Path to the image texture (PNG, JPG, DDS)
		bool fullscreen = true;               ///< If true, renders fullscreen. If false, uses position/size
		Ermine::Vec3 position = { 0.5f, 0.5f, 0.0f }; ///< Center position in normalized coordinates (0-1)
		float width = 1.0f;                   ///< Width in normalized coordinates (0-1)
		float height = 1.0f;                  ///< Height in normalized coordinates (0-1)
		Ermine::Vec3 tintColor = { 1.0f, 1.0f, 1.0f }; ///< Color tint (1,1,1 = no tint)
		float alpha = 1.0f;                   ///< Alpha transparency (0-1)
		bool maintainAspectRatio = true;      ///< Preserve image aspect ratio

		// Caption/Text overlay
		std::string caption = "";             ///< Caption text to display
		bool showCaption = false;             ///< If true, renders caption text
		Ermine::Vec3 captionColor = { 1.0f, 1.0f, 1.0f }; ///< Caption text color
		float captionFontSize = 24.0f;        ///< Caption font size
		Ermine::Vec3 captionPosition = { 0.5f, 0.1f, 0.0f }; ///< Caption position (bottom center by default)

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			out.AddMember("imagePath", rapidjson::Value(imagePath.c_str(), alloc), alloc);
			out.AddMember("fullscreen", fullscreen, alloc);
			out.AddMember("position", Vec3ToJson(position, alloc), alloc);
			out.AddMember("width", width, alloc);
			out.AddMember("height", height, alloc);
			out.AddMember("tintColor", Vec3ToJson(tintColor, alloc), alloc);
			out.AddMember("alpha", alpha, alloc);
			out.AddMember("maintainAspectRatio", maintainAspectRatio, alloc);
			out.AddMember("caption", rapidjson::Value(caption.c_str(), alloc), alloc);
			out.AddMember("showCaption", showCaption, alloc);
			out.AddMember("captionColor", Vec3ToJson(captionColor, alloc), alloc);
			out.AddMember("captionFontSize", captionFontSize, alloc);
			out.AddMember("captionPosition", Vec3ToJson(captionPosition, alloc), alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("imagePath") && in["imagePath"].IsString())
				imagePath = in["imagePath"].GetString();
			if (in.HasMember("fullscreen") && in["fullscreen"].IsBool())
				fullscreen = in["fullscreen"].GetBool();
			if (in.HasMember("position") && in["position"].IsArray())
				position = JsonToVec3(in["position"]);
			if (in.HasMember("width") && in["width"].IsNumber())
				width = in["width"].GetFloat();
			if (in.HasMember("height") && in["height"].IsNumber())
				height = in["height"].GetFloat();
			if (in.HasMember("tintColor") && in["tintColor"].IsArray())
				tintColor = JsonToVec3(in["tintColor"]);
			if (in.HasMember("alpha") && in["alpha"].IsNumber())
				alpha = in["alpha"].GetFloat();
			if (in.HasMember("maintainAspectRatio") && in["maintainAspectRatio"].IsBool())
				maintainAspectRatio = in["maintainAspectRatio"].GetBool();
			if (in.HasMember("caption") && in["caption"].IsString())
				caption = in["caption"].GetString();
			if (in.HasMember("showCaption") && in["showCaption"].IsBool())
				showCaption = in["showCaption"].GetBool();
			if (in.HasMember("captionColor") && in["captionColor"].IsArray())
				captionColor = JsonToVec3(in["captionColor"]);
			if (in.HasMember("captionFontSize") && in["captionFontSize"].IsNumber())
				captionFontSize = in["captionFontSize"].GetFloat();
			if (in.HasMember("captionPosition") && in["captionPosition"].IsArray())
				captionPosition = JsonToVec3(in["captionPosition"]);
		}

		XPROPERTY_DEF(
			"UIImageComponent", UIImageComponent,
			xproperty::obj_member<"imagePath", &UIImageComponent::imagePath>,
			xproperty::obj_member<"fullscreen", &UIImageComponent::fullscreen>,
			xproperty::obj_member<"position", &UIImageComponent::position>,
			xproperty::obj_member<"width", &UIImageComponent::width>,
			xproperty::obj_member<"height", &UIImageComponent::height>,
			xproperty::obj_member<"tintColor", &UIImageComponent::tintColor>,
			xproperty::obj_member<"alpha", &UIImageComponent::alpha>,
			xproperty::obj_member<"maintainAspectRatio", &UIImageComponent::maintainAspectRatio>,
			xproperty::obj_member<"caption", &UIImageComponent::caption>,
			xproperty::obj_member<"showCaption", &UIImageComponent::showCaption>,
			xproperty::obj_member<"captionColor", &UIImageComponent::captionColor>,
			xproperty::obj_member<"captionFontSize", &UIImageComponent::captionFontSize>,
			xproperty::obj_member<"captionPosition", &UIImageComponent::captionPosition>
		)
	};

	/*!***********************************************************************
	\brief
		Light Probe Volume Component defines a grid-based volume of light probes.
		Auto-generates probe entities within the volume bounds at specified spacing.
		Used for large-scale GI coverage in scenes.
	*************************************************************************/
	struct LightProbeVolumeComponent
	{
		glm::vec3 boundsMin{ -5.0f, -5.0f, -5.0f };   // Minimum corner of volume
		glm::vec3 boundsMax{ 5.0f, 5.0f, 5.0f };      // Maximum corner of volume
		bool isActive{ true };                        // Whether this probe contributes to lighting
		int captureResolution{ 64 };                 // Cubemap face resolution for capture
		int voxelResolution{ 64 };                   // Voxel grid resolution per axis
		int priority{ 0 };                            // Higher priority wins; equal priorities blend
		bool showGizmos{ true };                      // Visualize volume bounds in editor
		std::string bakedProbePath{};                // Path to baked probe data on disk

		// Runtime data (not serialized)
		glm::vec3 shCoefficients[9]{};                // L2 SH coefficients
		int probeIndex{ -1 };                         // Runtime index into probe cubemap array
		bool bakedDataLoaded{ false };                // Runtime: loaded SH from disk

		LightProbeVolumeComponent()
		{
			for (int i = 0; i < 9; ++i) {
				shCoefficients[i] = glm::vec3(0.0f);
			}
		}

		template<typename Alloc>
		void Serialize(rapidjson::Value& out, Alloc& alloc) const
		{
			out.SetObject();
			
			rapidjson::Value minArray(rapidjson::kArrayType);
			minArray.PushBack(boundsMin.x, alloc);
			minArray.PushBack(boundsMin.y, alloc);
			minArray.PushBack(boundsMin.z, alloc);
			out.AddMember("boundsMin", minArray, alloc);

			rapidjson::Value maxArray(rapidjson::kArrayType);
			maxArray.PushBack(boundsMax.x, alloc);
			maxArray.PushBack(boundsMax.y, alloc);
			maxArray.PushBack(boundsMax.z, alloc);
			out.AddMember("boundsMax", maxArray, alloc);

			out.AddMember("isActive", isActive, alloc);
			out.AddMember("captureResolution", captureResolution, alloc);
			out.AddMember("voxelResolution", voxelResolution, alloc);
			out.AddMember("priority", priority, alloc);
			out.AddMember("showGizmos", showGizmos, alloc);
			out.AddMember("bakedProbePath", rapidjson::Value(bakedProbePath.c_str(), alloc), alloc);
		}

		void Deserialize(const rapidjson::Value& in)
		{
			if (in.HasMember("boundsMin") && in["boundsMin"].IsArray()) {
				const auto& arr = in["boundsMin"].GetArray();
				if (arr.Size() >= 3) {
					boundsMin.x = arr[0].GetFloat();
					boundsMin.y = arr[1].GetFloat();
					boundsMin.z = arr[2].GetFloat();
				}
			}
			if (in.HasMember("boundsMax") && in["boundsMax"].IsArray()) {
				const auto& arr = in["boundsMax"].GetArray();
				if (arr.Size() >= 3) {
					boundsMax.x = arr[0].GetFloat();
					boundsMax.y = arr[1].GetFloat();
					boundsMax.z = arr[2].GetFloat();
				}
			}
			if (in.HasMember("isActive") && in["isActive"].IsBool())
				isActive = in["isActive"].GetBool();
			if (in.HasMember("captureResolution") && in["captureResolution"].IsInt())
				captureResolution = in["captureResolution"].GetInt();
			if (in.HasMember("voxelResolution") && in["voxelResolution"].IsInt())
				voxelResolution = in["voxelResolution"].GetInt();
			if (in.HasMember("priority") && in["priority"].IsInt())
				priority = in["priority"].GetInt();
			if (in.HasMember("showGizmos") && in["showGizmos"].IsBool())
				showGizmos = in["showGizmos"].GetBool();
			if (in.HasMember("bakedProbePath") && in["bakedProbePath"].IsString())
				bakedProbePath = in["bakedProbePath"].GetString();
		}

		XPROPERTY_DEF(
			"LightProbeVolumeComponent", LightProbeVolumeComponent,
			xproperty::obj_member<"boundsMin", &LightProbeVolumeComponent::boundsMin>,
			xproperty::obj_member<"boundsMax", &LightProbeVolumeComponent::boundsMax>,
			xproperty::obj_member<"isActive", &LightProbeVolumeComponent::isActive>,
			xproperty::obj_member<"captureResolution", &LightProbeVolumeComponent::captureResolution>,
			xproperty::obj_member<"voxelResolution", &LightProbeVolumeComponent::voxelResolution>,
			xproperty::obj_member<"priority", &LightProbeVolumeComponent::priority>,
			xproperty::obj_member<"showGizmos", &LightProbeVolumeComponent::showGizmos>,
			xproperty::obj_member<"bakedProbePath", &LightProbeVolumeComponent::bakedProbePath>
		)
	};
} // namespace Ermine
