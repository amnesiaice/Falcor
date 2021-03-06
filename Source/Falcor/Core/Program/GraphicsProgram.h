/***************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#pragma once
#include "Program.h"

namespace Falcor
{
    /** Graphics program. See ComputeProgram to manage compute shaders.
    */
    class dlldecl GraphicsProgram : public Program, inherit_shared_from_this<Program, GraphicsProgram>
    {
    public:
        using SharedPtr = std::shared_ptr<GraphicsProgram>;
        using SharedConstPtr = std::shared_ptr<const GraphicsProgram>;
        using inherit_shared_from_this<Program, GraphicsProgram>::shared_from_this;

        ~GraphicsProgram() = default;

        /** Create a new program object.
            \param[in] desc Description of the source files and entry points to use.
            \return A new object, or nullptr if creation failed.

            Note that this call merely creates a program object. The actual compilation and link happens when calling Program#getActiveVersion().
        */
        static SharedPtr create(const Desc& desc, const Program::DefineList& programDefines = DefineList());

        /** Create a new object.
            \param[in] filename Shaders filename.
            \param[in] vsEntry Vertex-shader entry point. If this string is empty (""), it will use a default vertex shader which transforms and outputs all default vertex attributes.
            \param[in] psEntry Pixel shader entry point
            \param[in] programDefines A list of macro definitions to set into the shaders. The macro definitions will be assigned to all the shaders.
            \return A new object, or nullptr if creation failed.
        */
        static SharedPtr createFromFile(const std::string& filename, const std::string& vsEntry, const std::string& psEntry, const DefineList& programDefines = DefineList());

    private:
        GraphicsProgram() = default;
    };
}
