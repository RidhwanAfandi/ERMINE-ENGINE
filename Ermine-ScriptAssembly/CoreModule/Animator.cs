/* Start Header ************************************************************************/
/*!
\file       Animator.cs
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       04/02/2026
\brief      Managed Animator component API (Unity-style).
            Exposes animation parameters and state control.
            Backed by native AnimationComponent + AnimationGraph.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class Animator : Component
    {
        #region InternalCalls

        // --- Bool ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern bool Internal_GetBool(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_SetBool(string name, bool value);

        // --- Float ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern float Internal_GetFloat(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_SetFloat(string name, float value);

        // --- Int ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern int Internal_GetInt(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_SetInt(string name, int value);

        // --- Trigger ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_SetTrigger(string name);

        // --- State ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern string Internal_GetCurrentStateName();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_SetState(string stateName);

        #endregion

        // --- Public API ---
        public bool GetBool(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.GetBool called with null or empty name");
                return false;
            }

            return Internal_GetBool(name);
        }

        public void SetBool(string name, bool value)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.SetBool called with null or empty name");
                return;
            }

            Internal_SetBool(name, value);
        }

        public float GetFloat(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.GetFloat called with null or empty name");
                return 0f;
            }

            return Internal_GetFloat(name);
        }

        public void SetFloat(string name, float value)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.SetFloat called with null or empty name");
                return;
            }

            Internal_SetFloat(name, value);
        }

        public int GetInt(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.GetInt called with null or empty name");
                return 0;
            }

            return Internal_GetInt(name);
        }

        public void SetInt(string name, int value)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.SetInt called with null or empty name");
                return;
            }

            Internal_SetInt(name, value);
        }

        public void SetTrigger(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogWarning("Animator.SetTrigger called with null or empty name");
                return;
            }

            Internal_SetTrigger(name);
        }

        public string currentState
        {
            get
            {
                string state = Internal_GetCurrentStateName();
                if (state == null)
                    Debug.LogWarning($"Animator has no active state on {gameObject?.name ?? "unknown"}");
                return state;
            }
        }

        public void Play(string stateName)
        {
            if (string.IsNullOrEmpty(stateName))
            {
                Debug.LogWarning("Animator.Play called with null or empty state name");
                return;
            }

            Internal_SetState(stateName);
        }

        public void PlayStartState()
        {
            string state = currentState;
            if (!string.IsNullOrEmpty(state))
                Play(state);
        }
    }
}
