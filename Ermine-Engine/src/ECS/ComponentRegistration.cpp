/* Start Header ************************************************************************/
/*!
\file       ComponentRegistration.cpp
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Sep 30, 2025
\brief      This file contains the definition of the member function of ComponentManager.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"

#include "Components.h"
#include "xcore/my_properties.h"  
#include "xproperty.h"

using Ermine::MeshPrimitiveDesc;
using Ermine::MeshAssetDesc;
using Ermine::Mesh;

namespace Ermine 
{
	XPROPERTY_REG(Transform)
	XPROPERTY_REG(ObjectMetaData)
	XPROPERTY_REG(Script)
	XPROPERTY_REG(ScriptsComponent)
	XPROPERTY_REG(MeshPrimitiveDesc)
	XPROPERTY_REG(MeshAssetDesc)
	XPROPERTY_REG(Mesh)
	XPROPERTY_REG(Material)
	XPROPERTY_REG(Light)
	XPROPERTY_REG(AudioSource)
	XPROPERTY_REG(GlobalAudioComponent)
	XPROPERTY_REG(AudioComponent)
	XPROPERTY_REG(PhysicComponent)
	XPROPERTY_REG(GlobalTransform)
	XPROPERTY_REG(ParticleEmitter)
	XPROPERTY_REG(GPUParticleEmitter)
	XPROPERTY_REG(AABBComponent)
	XPROPERTY_REG(HierarchyComponent)
	XPROPERTY_REG(UIComponent)
	XPROPERTY_REG(UIHealthbarComponent)
	XPROPERTY_REG(UICrosshairComponent)
	XPROPERTY_REG(UISkillsComponent)
	XPROPERTY_REG(UIManaBarComponent)
	XPROPERTY_REG(UIBookCounterComponent)
	XPROPERTY_REG(UISliderComponent)
	XPROPERTY_REG(UITextComponent)
	XPROPERTY_REG(GlobalGraphics)
	XPROPERTY_REG(LightProbeVolumeComponent)

	//XPROPERTY_REG(ModelComponent)
	//XPROPERTY_REG(AnimationComponent)
}