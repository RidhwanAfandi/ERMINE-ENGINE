/* Start Header ************************************************************************/
/*!
\file       InspectorGUI.cpp
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Sep 01, 2025
\brief      This file contains the implementation of the Inspector GUI window.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "InspectorGUI.h"
#include <imgui.h>
#include "MathVector.h"
#include "Components.h"
#include "Physics.h"

namespace Ermine
{
    InspectorGUI::InspectorGUI()
        : ImGUIWindow("Inspector"), m_entity(EntityID{})
    {
    }

    InspectorGUI::InspectorGUI(EntityID entity, std::string name)
        : ImGUIWindow(name), m_entity(entity)
    {
    }

    void InspectorGUI::SetEntity(EntityID entity)
    {
        m_entity = entity;

    }

    void InspectorGUI::Render()
    {
        //m_Entities (list of entities)

   //     if (!ImGui::Begin(Name().c_str()))
   //     {
   //         ImGui::End();
   //         return;
   //     }

   //     auto& ecs = ECS::GetInstance();
   //     if (!ecs.IsEntityValid(m_entity))
   //     {
   //         ImGui::TextDisabled("No GameObject selected");
   //         ImGui::End();
   //         return;
   //     }

   //     // Transform Component
   //     if (ecs.HasComponent<Transform>(m_entity))
   //     {
   //         auto& tr = ecs.GetComponent<Transform>(m_entity);

   //         if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
   //         {
   //             ImGui::BeginChild("TransformChild", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
   //             //ImGui::BeginGroup();
   //             // Position
   //             ImGui::TextUnformatted("Position");
   //             ImGui::SameLine();
   //             if (ImGui::SmallButton("Reset##pos")) tr.position = Vector3D(0.f, 0.f, 0.f);
   //             ImGui::DragFloat3("##pos", &tr.position.x);

   //             // Rotation
   //             ImGui::TextUnformatted("Rotation");
   //             ImGui::SameLine();
   //             if (ImGui::SmallButton("Reset##rot")) tr.rotation = Quaternion(0.f, 0.f, 0.f, 1.f);

   //             Vector3D eulerDeg = QuaternionToEuler(tr.rotation);
   //             
   //             if (ImGui::DragFloat3("##rot", &eulerDeg.x, 1.0f))
   //                 tr.rotation = EulerDegToQuaternion(eulerDeg);

   //             // Scale
   //             ImGui::TextUnformatted("Scale");
   //             ImGui::SameLine();


   //             if (ImGui::SmallButton("Reset##scl")) tr.scale = Vector3D(1.f, 1.f, 1.f);
   //             if (ImGui::DragFloat3("##scl", &tr.scale.x))
   //             {
   //                 constexpr float kMinScale = 0.0001f;
   //                 tr.scale.x = (tr.scale.x >= 0.f) ? fmaxf(tr.scale.x, kMinScale) : -fmaxf(-tr.scale.x, kMinScale);
   //                 tr.scale.y = (tr.scale.y >= 0.f) ? fmaxf(tr.scale.y, kMinScale) : -fmaxf(-tr.scale.y, kMinScale);
   //                 tr.scale.z = (tr.scale.z >= 0.f) ? fmaxf(tr.scale.z, kMinScale) : -fmaxf(-tr.scale.z, kMinScale);
   //             }

   //             // Context menu for contents
   //             if (ImGui::BeginPopupContextWindow("Transform_ContentContext", ImGuiPopupFlags_MouseButtonRight))
   //             {
   //                 if (ImGui::MenuItem("Delete", "Del", false))
   //                 {
   //                     ecs.RemoveComponent<Transform>(m_entity);
   //                 }
   //                 ImGui::EndPopup();
   //             }

   //             //ImGui::EndGroup();
   //             ImGui::EndChild();

   //         }

   //         ImGui::Separator();
   //     }

   //     if (ecs.HasComponent<Material>(m_entity))
   //     {
   //         auto& mt = ecs.GetComponent<Material>(m_entity);

   //         if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
   //         {
   //         }

   //         ImGui::Separator();
   //     }

   //     if (ecs.HasComponent<AudioComponent>(m_entity))
   //     {
   //         auto& ac = ecs.GetComponent<AudioComponent>(m_entity);

   //         if (ImGui::CollapsingHeader("AudioComponent", ImGuiTreeNodeFlags_DefaultOpen))
   //         {
   //         }

   //         ImGui::Separator();
   //     }

   //     if (ecs.HasComponent<PhysicComponent>(m_entity))
   //     {
   //         auto& pc = ecs.GetComponent<PhysicComponent>(m_entity);

   //         if (ImGui::CollapsingHeader("PhysicComponent", ImGuiTreeNodeFlags_DefaultOpen))
   //         {
   //             const char* PhysicsBody[] = {"Rigidbody", "Trigger"};

   //             if (ImGui::BeginCombo("Physics Body Type", PhysicsBody[(int)pc.bodyType]))
   //             {
   //                 for (int n = 0; n < 2; n++)
   //                 {
   //                     const bool is_selected = (pc.bodyType == (PhysicsBodyType)n);
   //                     if (ImGui::Selectable(PhysicsBody[n], is_selected))
   //                     {
   //                         pc.bodyType = (PhysicsBodyType)n;
   //                         ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
   //                     }

   //                     // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
   //                     if (is_selected)
   //                         ImGui::SetItemDefaultFocus();
   //                 }
   //                 ImGui::EndCombo();
   //             }

   //             const char* EmotionType[] = {"Static", "Kinematic", "Dynamic"};

   //             if (ImGui::BeginCombo("Emotion Type", EmotionType[(int)pc.motionType]))
   //             {
   //                 for (int n = 0; n < 3; n++)
   //                 {
   //                     const bool is_selected = (pc.motionType == (JPH::EMotionType)n);
   //                     if (ImGui::Selectable(EmotionType[n], is_selected))
   //                     {
   //                         pc.motionType = (JPH::EMotionType)n;
   //                         ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
   //                     }

   //                     if (is_selected)
   //                         ImGui::SetItemDefaultFocus();
   //                 }
   //                 ImGui::EndCombo();
   //             }

   //             if (ImGui::DragFloat("##mas", &pc.mass))
   //             {
   //                 constexpr float kMinScale = 0.f;
   //                 pc.mass = fmaxf(pc.mass, kMinScale);
   //                 ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
   //             }

   //             const char* ShapeTypeList[] = { "Box", "Sphere", "Capsule", "CustomMesh" };

   //             if (ImGui::BeginCombo("Shape Type", ShapeTypeList[(int)pc.shapeType]))
   //             {
   //                 for (int n = 0; n < (int)ShapeType::Total; n++)
   //                 {
   //                     const bool is_selected = (pc.shapeType == (ShapeType)n);
   //                     if (ImGui::Selectable(ShapeTypeList[n], is_selected))
   //                     {
   //                         pc.shapeType = (ShapeType)n;
   //                         ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
   //                     }

   //                     if (is_selected)
   //                         ImGui::SetItemDefaultFocus();
   //                 }
   //                 ImGui::EndCombo();
   //             }
   //         }

   //         ImGui::Separator();
   //     }

   //     if (ImGui::Button("Add Component"))
   //     {
			//ImGui::OpenPopup("AddComponent");
   //     }

   //     ImGui::End();
    }
}
