// Filename: LDataManager-inl.h
// Created on 10 Dec 2009 by Boyce Griffith
//
// Copyright (c) 2002-2014, Boyce Griffith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of New York University nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef included_LDataManager_inl_h
#define included_LDataManager_inl_h

/////////////////////////////// INCLUDES /////////////////////////////////////

#include "ibtk/LData.h"
#include "ibtk/LDataManager.h"
#include "ibtk/LMesh.h"

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// PUBLIC ///////////////////////////////////////

inline const SAMRAI::hier::IntVector&
LDataManager::getGhostCellWidth() const
{
    return d_ghost_width;
}// getGhostCellWidth

inline const std::string&
LDataManager::getDefaultInterpKernelFunction() const
{
    return d_default_interp_kernel_fcn;
}// getDefaultInterpKernelFunction

inline const std::string&
LDataManager::getDefaultSpreadKernelFunction() const
{
    return d_default_spread_kernel_fcn;
}// getDefaultSpreadKernelFunction

inline bool
LDataManager::levelContainsLagrangianData(
    const int level_number) const
{
    int ret_val = 0;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_level_contains_lag_data[level_number];
    }
    return ret_val;
}// levelContainsLagrangianData

inline unsigned int
LDataManager::getNumberOfNodes(
    const int level_number) const
{
    int ret_val = 0;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_num_nodes[level_number];
    }
    return ret_val;
}// getNumberOfNodes

inline unsigned int
LDataManager::getNumberOfLocalNodes(
    const int level_number) const
{
    int ret_val = 0;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_local_lag_indices[level_number].size();
    }
    return ret_val;
}// getNumberOfLocalNodes

inline unsigned int
LDataManager::getNumberOfGhostNodes(
    const int level_number) const
{
    int ret_val = 0;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_nonlocal_lag_indices[level_number].size();
    }
    return ret_val;
}// getNumberOfGhostNodes

inline unsigned int
LDataManager::getGlobalNodeOffset(
    const int level_number) const
{
    int ret_val = 0;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_node_offset[level_number];
    }
    return ret_val;
}// getGlobalNodeOffset

inline boost::shared_ptr<LMesh>
LDataManager::getLMesh(
    const int level_number) const
{
    boost::shared_ptr<LMesh> ret_val;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        ret_val = d_lag_mesh[level_number];
    }
    return ret_val;
}// getLMesh

inline boost::shared_ptr<LData>
LDataManager::getLData(
    const std::string& quantity_name,
    const int level_number) const
{
    boost::shared_ptr<LData> ret_val;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        TBOX_ASSERT(d_lag_mesh_data[level_number].find(quantity_name) != d_lag_mesh_data[level_number].end());
        ret_val = d_lag_mesh_data[level_number].find(quantity_name)->second;
    }
    return ret_val;
}// getLData

inline int
LDataManager::getLNodePatchDescriptorIndex() const
{
    return d_lag_node_index_current_idx;
}// getLNodePatchDescriptorIndex

inline int
LDataManager::getWorkloadPatchDescriptorIndex() const
{
    return d_workload_idx;
}// getWorkloadPatchDescriptorIndex

inline int
LDataManager::getNodeCountPatchDescriptorIndex() const
{
    return d_node_count_idx;
}// getNodeCountPatchDescriptorIndex

inline std::vector<std::string>
LDataManager::getLagrangianStructureNames(
    const int level_number) const
{
    std::vector<std::string> ret_val;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        for (const auto& elem : d_strct_id_to_strct_name_map[level_number])
        {
            ret_val.push_back(elem.second);
        }
    }
    return ret_val;
}// getLagrangianStructureNames

inline std::vector<int>
LDataManager::getLagrangianStructureIDs(
    const int level_number) const
{
    std::vector<int> ret_val;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        for (const auto& elem : d_strct_name_to_strct_id_map[level_number])
        {
            ret_val.push_back(elem.second);
        }
    }
    return ret_val;
}// getLagrangianStructureIDs

inline int
LDataManager::getLagrangianStructureID(
    const int lagrangian_index,
    const int level_number) const
{
    int ret_val = -1;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        const auto& it = d_last_lag_idx_to_strct_id_map[level_number].lower_bound(lagrangian_index);
        if (LIKELY(it != d_last_lag_idx_to_strct_id_map[level_number].end())) ret_val = it->second;
    }
    return ret_val;
}// getLagrangianStructureID

inline int
LDataManager::getLagrangianStructureID(
    const std::string& structure_name,
    const int level_number) const
{
    int ret_val = -1;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        const auto& it = d_strct_name_to_strct_id_map[level_number].find(structure_name);
        if (LIKELY(it != d_strct_name_to_strct_id_map[level_number].end())) ret_val = it->second;
    }
    return ret_val;
}// getLagrangianStructureID

inline std::string
LDataManager::getLagrangianStructureName(
    const int structure_id,
    const int level_number) const
{
    std::string ret_val = "UNKNOWN";
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        const auto& it = d_strct_id_to_strct_name_map[level_number].find(structure_id);
        if (LIKELY(it != d_strct_id_to_strct_name_map[level_number].end())) ret_val = it->second;
    }
    return ret_val;
}// getLagrangianStructureName

inline std::pair<int,int>
LDataManager::getLagrangianStructureIndexRange(
    const int structure_id,
    const int level_number) const
{
    std::pair<int,int> ret_val = std::make_pair(-1,-1);
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        const auto& it = d_strct_id_to_lag_idx_range_map[level_number].find(structure_id);
        if (LIKELY(it != d_strct_id_to_lag_idx_range_map[level_number].end())) ret_val = it->second;
    }
    return ret_val;
}// getLagrangianStructureIndexRange

inline bool
LDataManager::getLagrangianStructureIsActivated(
    const int structure_id,
    const int level_number) const
{
    bool ret_val = false;
    if (d_coarsest_ln <= level_number &&
        d_finest_ln   >= level_number)
    {
        const auto& idx_range = getLagrangianStructureIndexRange(structure_id, level_number);
        if (structure_id >= idx_range.first && structure_id < idx_range.second)
        {
            const auto& it = d_inactive_strcts[level_number].getSet().find(structure_id);
            if (it == d_inactive_strcts[level_number].getSet().end()) ret_val = true;
        }
    }
    return ret_val;
}// getLagrangianStructureIsActivated

/////////////////////////////// PRIVATE //////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_LDataManager_inl_h
