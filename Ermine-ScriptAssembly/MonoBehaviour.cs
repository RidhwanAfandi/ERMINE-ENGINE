/* Start Header ************************************************************************/
/*!
\file       MonoBehaviour.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       03/09/2025
\brief      This file contains the MonoBehaviour class which serves as a base class for user-defined scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace ErmineEngine
{
    [System.AttributeUsage(System.AttributeTargets.Field, Inherited = false, AllowMultiple = false)]
    public sealed class SerializeFieldAttribute : System.Attribute
    {
    }

    public class MonoBehaviour : Component
    {
        
    }
}
