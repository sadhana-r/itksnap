/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: GenericImageData.cxx,v $
  Language:  C++
  Date:      $Date: 2010/06/28 18:45:08 $
  Version:   $Revision: 1.14 $
  Copyright (c) 2007 Paul A. Yushkevich
  
  This file is part of ITK-SNAP 

  ITK-SNAP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  -----

  Copyright (c) 2003 Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information. 

=========================================================================*/
// ITK Includes
#include "itkImage.h"
#include "itkImageIterator.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkUnaryFunctorImageFilter.h"
#include "itkRGBAPixel.h"
#include "IRISSlicer.h"
#include "IRISException.h"
#include "IRISApplication.h"
#include <algorithm>
#include <list>
#include <map>
#include <iostream>
#include "SNAPEventListenerCallbacks.h"
#include "GenericImageData.h"
#include "Rebroadcaster.h"

// System includes
#include <fstream>
#include <iostream>
#include <iomanip>


void 
GenericImageData
::SetSegmentationVoxel(const Vector3ui &index, LabelType value)
{
  // Make sure that the main image data and the segmentation data exist
  assert(IsSegmentationLoaded());

  // Store the voxel
  m_LabelWrapper->SetVoxel(index, value);

  // Mark the image as modified
  m_LabelWrapper->GetImage()->Modified();
}

GenericImageData
::GenericImageData()
{
  // Make main image wrapper point to grey wrapper initially
  m_MainImageWrapper = NULL;

  // Pass the label table from the parent to the label wrapper
  m_LabelWrapper = NULL;
  
  // Add to the relevant lists
  m_Wrappers[LayerIterator::MAIN_ROLE].push_back(m_MainImageWrapper);
  m_Wrappers[LayerIterator::LABEL_ROLE].push_back(m_LabelWrapper.GetPointer());
}

GenericImageData
::~GenericImageData()
{
  UnloadMainImage();
}

Vector3d 
GenericImageData
::GetImageSpacing() 
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetImageBase()->GetSpacing().GetVnlVector();
}

Vector3d 
GenericImageData
::GetImageOrigin() 
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetImageBase()->GetOrigin().GetVnlVector();
}


void
GenericImageData
::SetMainImage(AnatomicImageType *image,
               const ImageCoordinateGeometry &newGeometry,
               const LinearInternalToNativeIntensityMapping &native)
{
  // Create a main wrapper of fixed type.
  SmartPtr<AnatomicImageWrapper> wrapper = AnatomicImageWrapper::New();

  // Set properties
  wrapper->SetImage(image);
  wrapper->SetNativeMapping(native);

  // Make the wrapper the main image
  SetSingleImageWrapper(LayerIterator::MAIN_ROLE, wrapper.GetPointer());
  m_MainImageWrapper = wrapper;

  // Initialize the segmentation data to zeros
  m_LabelWrapper = LabelImageWrapper::New();
  m_LabelWrapper->InitializeToWrapper(m_MainImageWrapper, (LabelType) 0);

  m_LabelWrapper->GetDisplayMapping()->SetLabelColorTable(m_Parent->GetColorLabelTable());
  SetSingleImageWrapper(LayerIterator::LABEL_ROLE, m_LabelWrapper.GetPointer());

  // Set opaque
  m_MainImageWrapper->SetAlpha(255);

  // Pass the coordinate transform to the wrappers
  SetImageGeometry(newGeometry);
}

void
GenericImageData
::UnloadMainImage()
{
  // First unload the overlays if exist
  UnloadOverlays();

  // Clear the main image wrappers
  RemoveSingleImageWrapper(LayerIterator::MAIN_ROLE);
  m_MainImageWrapper = NULL;

  // Reset the label wrapper
  RemoveSingleImageWrapper(LayerIterator::LABEL_ROLE);
  m_LabelWrapper = NULL;
}

void
GenericImageData
::AddOverlay(AnatomicImageType *image,
             const LinearInternalToNativeIntensityMapping &native)
{
  // Check that the image matches the size of the main image
  //Octavian_2012_08_24_16:20: changed assert into this test as a response to:
  //bug: ID: 3023489: "-o flag size check" 
  if(m_MainImageWrapper->GetBufferedRegion() != image->GetBufferedRegion())
    {
    throw IRISException("Main and overlay data sizes are different");
    }

  // Pass the image to a Grey image wrapper
  SmartPtr<AnatomicImageWrapper> overlay = AnatomicImageWrapper::New();
  overlay->SetImage(image);
  overlay->SetNativeMapping(native);
  overlay->SetAlpha(0.5);

  // Sync up spacing between the main and overlay image
  overlay->GetImageBase()->SetSpacing(
        m_MainImageWrapper->GetImageBase()->GetSpacing());

  overlay->GetImageBase()->SetOrigin(
        m_MainImageWrapper->GetImageBase()->GetOrigin());

  overlay->GetImageBase()->SetDirection(
        m_MainImageWrapper->GetImageBase()->GetDirection());

  // Propagate the geometry information to this wrapper
  for(unsigned int iSlice = 0; iSlice < 3; iSlice ++)
    {
    overlay->SetImageToDisplayTransform(
      iSlice, m_ImageGeometry.GetImageToDisplayTransform(iSlice));
    }

  // Add to the overlay wrapper list
  PushBackImageWrapper(LayerIterator::OVERLAY_ROLE, overlay.GetPointer());
}

void
GenericImageData
::UnloadOverlays()
{
  while (m_Wrappers[LayerIterator::OVERLAY_ROLE].size() > 0)
    UnloadOverlayLast();
}

void
GenericImageData
::UnloadOverlayLast()
{
  // Make sure at least one grey overlay is loaded
  if (!IsOverlayLoaded())
    return;

  // Release the data associated with the last overlay
  PopBackImageWrapper(LayerIterator::OVERLAY_ROLE);
}

void GenericImageData
::UnloadOverlay(ImageWrapperBase *overlay)
{
  // Erase the overlay
  WrapperList &overlays = m_Wrappers[LayerIterator::OVERLAY_ROLE];
  WrapperIterator it =
      std::find(overlays.begin(), overlays.end(), overlay);
  if(it != overlays.end())
    overlays.erase(it);
}

void
GenericImageData
::SetSegmentationImage(LabelImageType *newLabelImage) 
{
  // Check that the image matches the size of the grey image
  assert(m_MainImageWrapper->IsInitialized() &&
    m_MainImageWrapper->GetBufferedRegion() == 
         newLabelImage->GetBufferedRegion());

  // Pass the image to the segmentation wrapper (why this and not create a
  // new label wrapper? Why should a wrapper have longer lifetime than an
  // image that it wraps around
  m_LabelWrapper->SetImage(newLabelImage);

  // Sync up spacing between the main and label image
  newLabelImage->SetSpacing(m_MainImageWrapper->GetImageBase()->GetSpacing());
  newLabelImage->SetOrigin(m_MainImageWrapper->GetImageBase()->GetOrigin());
}

bool
GenericImageData
::IsOverlayLoaded()
{
  return (m_Wrappers[LayerIterator::OVERLAY_ROLE].size() > 0);
}

bool
GenericImageData
::IsSegmentationLoaded()
{
  return m_LabelWrapper && m_LabelWrapper->IsInitialized();
}

void
GenericImageData
::SetCrosshairs(const Vector3ui &crosshairs)
{
  // Set crosshairs in all wrappers
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    if(lit.GetLayer() && lit.GetLayer()->IsInitialized())
      lit.GetLayer()->SetSliceIndex(crosshairs);
}

GenericImageData::RegionType
GenericImageData
::GetImageRegion() const
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetBufferedRegion();
}

void
GenericImageData
::SetImageGeometry(const ImageCoordinateGeometry &geometry)
{
  m_ImageGeometry = geometry;
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    if(lit.GetLayer() && lit.GetLayer()->IsInitialized())
      {
      // Set the direction matrix in the image
      lit.GetLayer()->SetImageGeometry(geometry);
      }
}

unsigned int GenericImageData::GetNumberOfLayers(int role_filter)
{
  unsigned int n = 0;

  LayerIterator it = this->GetLayers(role_filter);
  while(!it.IsAtEnd())
    {
    n++; ++it;
    }

  return n;
}

ImageWrapperBase *
GenericImageData
::FindLayer(unsigned long unique_id, bool search_derived, int role_filter)
{
  for(LayerIterator it = this->GetLayers(role_filter); !it.IsAtEnd(); ++it)
    {
    if(it.GetLayer()->GetUniqueId() == unique_id)
      {
      return it.GetLayer();
      }
    else if(search_derived)
      {
      VectorImageWrapperBase *vec = it.GetLayerAsVector();
      if(vec)
        {
        for(int j = SCALAR_REP_COMPONENT; j < NUMBER_OF_SCALAR_REPS; j++)
          {
          int n = (j == SCALAR_REP_COMPONENT) ? vec->GetNumberOfComponents() : 1;
          for(int k = 0; k < n; k++)
            {
            ImageWrapperBase *w = vec->GetScalarRepresentation((ScalarRepresentation) j, k);
            if(w && w->GetUniqueId() == unique_id)
              return w;
            }
          }
        }
      }
    }

  return NULL;
}

ImageWrapperBase *GenericImageData::GetLastOverlay()
{
  return m_Wrappers[LayerIterator::OVERLAY_ROLE].back();
}



void GenericImageData::PushBackImageWrapper(LayerRole role,
                                            ImageWrapperBase *wrapper)
{
  // Append the wrapper
  m_Wrappers[role].push_back(wrapper);

  // Rebroadcast the wrapper-related events as our own events
  Rebroadcaster::RebroadcastAsSourceEvent(wrapper, WrapperChangeEvent(), this);
}


void GenericImageData::PopBackImageWrapper(LayerRole role)
{
  m_Wrappers[role].pop_back();
}

void GenericImageData::MoveLayer(ImageWrapperBase *layer, int direction)
{
  // Find the layer
  LayerIterator it(this);
  it.Find(layer);
  if(!it.IsAtEnd())
    {
    WrapperList &wl = m_Wrappers[it.GetRole()];
    int k = it.GetPositionInRole();

    // Make sure the operation is legal!
    assert(k + direction >= 0 && k + direction < wl.size());

    // Do the swap
    std::swap(wl[k], wl[k+direction]);
    }
}

void GenericImageData::RemoveImageWrapper(LayerRole role,
                                          ImageWrapperBase *wrapper)
{
  m_Wrappers[role].erase(
        std::find(m_Wrappers[role].begin(), m_Wrappers[role].end(), wrapper));
}

void GenericImageData::SetSingleImageWrapper(LayerRole role,
                                             ImageWrapperBase *wrapper)
{
  assert(m_Wrappers[role].size() == 1);
  m_Wrappers[role].front() = wrapper;

  // Rebroadcast the wrapper-related events as our own events
  Rebroadcaster::RebroadcastAsSourceEvent(wrapper, WrapperChangeEvent(), this);
}

void GenericImageData::RemoveSingleImageWrapper(LayerRole role)
{
  assert(m_Wrappers[role].size() == 1);
  m_Wrappers[role].front() = NULL;
}







LayerIterator
::LayerIterator(
    GenericImageData *data, int role_filter)
{
  // Store the source information
  m_ImageData = data;
  m_RoleFilter = role_filter;

  // Populate role names
  if(m_RoleDefaultNames.size() == 0)
    {
    m_RoleDefaultNames.insert(std::make_pair(MAIN_ROLE, "Main Image"));
    m_RoleDefaultNames.insert(std::make_pair(OVERLAY_ROLE, "Overlay"));
    m_RoleDefaultNames.insert(std::make_pair(LABEL_ROLE, "Segmentation"));
    m_RoleDefaultNames.insert(std::make_pair(SNAP_ROLE, "SNAP Image"));
    }

  // Move to the beginning
  MoveToBegin();
}

LayerIterator& LayerIterator
::MoveToBegin()
{
  // Initialize to point to the first wrapper in the first role, even if
  // this is an invalid configuration
  m_RoleIter = m_ImageData->m_Wrappers.begin();
  if(m_RoleIter != m_ImageData->m_Wrappers.end())
    m_WrapperInRoleIter = m_RoleIter->second.begin();

  // Move up until we find a valid role or end
  while(!IsAtEnd() && !IsPointingToListableLayer())
    {
    MoveToNextTrialPosition();
    }

  return *this;
}

bool LayerIterator
::IsAtEnd() const
{
  // We are at end when there are no roles left
  return m_RoleIter == m_ImageData->m_Wrappers.end();
}


LayerIterator& LayerIterator
::MoveToEnd()
{
  m_RoleIter = m_ImageData->m_Wrappers.end();
  return *this;
}

LayerIterator& LayerIterator
::Find(ImageWrapperBase *value)
{
  // Just a linear search - we won't have so many wrappers!
  MoveToBegin();
  while(!this->IsAtEnd() && this->GetLayer() != value)
    ++(*this);
  return *this;
}

void LayerIterator::MoveToNextTrialPosition()
{
  // If we are at the end of storage, that's it
  if(m_RoleIter == m_ImageData->m_Wrappers.end())
    return;

  // If we are at the end of a chain of wrappers, or if the current role
  // is not a valid role, go to the start of the next role
  else if(m_WrapperInRoleIter == m_RoleIter->second.end() ||
     !(m_RoleFilter & m_RoleIter->first))
    {
    ++m_RoleIter;
    if(m_RoleIter != m_ImageData->m_Wrappers.end())
      m_WrapperInRoleIter = m_RoleIter->second.begin();

    }

  // Otherwise, advance the iterator in the wrapper chain
  else
    ++m_WrapperInRoleIter;
}

bool LayerIterator::IsPointingToListableLayer() const
{
  // I split this up for debugging

  // Are we at end of roles?
  if(m_RoleIter == m_ImageData->m_Wrappers.end())
    return false;

  // Are we in a valid role?
  GenericImageData::LayerRole lr = m_RoleIter->first;
  if((m_RoleFilter & lr) == 0)
    return false;

  // In our role, are we at the end?
  if(m_WrapperInRoleIter == m_RoleIter->second.end())
    return false;

  // Is the layer null?
  if((*m_WrapperInRoleIter).IsNull())
    return false;

  return true;
}

LayerIterator &
LayerIterator::operator ++()
{
  do
    {
    MoveToNextTrialPosition();
    }
  while(!IsAtEnd() && !IsPointingToListableLayer());

  return *this;
}

LayerIterator &
LayerIterator::operator +=(int k)
{
  for(int i = 0; i < k; i++)
    ++(*this);
  return *this;
}

ImageWrapperBase * LayerIterator::GetLayer() const
{
  assert(IsPointingToListableLayer());
  return (*m_WrapperInRoleIter);
}

ScalarImageWrapperBase * LayerIterator::GetLayerAsScalar() const
{
  return dynamic_cast<ScalarImageWrapperBase *>(this->GetLayer());
}

VectorImageWrapperBase * LayerIterator::GetLayerAsVector() const
{
  return dynamic_cast<VectorImageWrapperBase *>(this->GetLayer());
}

LayerIterator::LayerRole
LayerIterator::GetRole() const
{
  assert(IsPointingToListableLayer());
  return m_RoleIter->first;
}

int LayerIterator::GetPositionInRole() const
{
  return (int)(m_WrapperInRoleIter - m_RoleIter->second.begin());
}

int LayerIterator::GetNumberOfLayersInRole()
{
  assert(IsPointingToListableLayer());
  return m_RoleIter->second.size();
}

bool LayerIterator::IsFirstInRole() const
{
  assert(IsPointingToListableLayer());
  int pos = m_WrapperInRoleIter - m_RoleIter->second.begin();
  return (pos == 0);
}

bool LayerIterator::IsLastInRole() const
{
  assert(IsPointingToListableLayer());
  int pos = m_WrapperInRoleIter - m_RoleIter->second.begin();
  return (pos == m_RoleIter->second.size() - 1);
}

bool LayerIterator::operator ==(const LayerIterator &it)
{
  // Two iterators are equal if they both point to the same location
  // or both are at the end.
  if(this->IsAtEnd())
    return it.IsAtEnd();
  else if(it.IsAtEnd())
    return false;
  else
    return this->GetLayer() == it.GetLayer();
}

bool LayerIterator::operator !=(const LayerIterator &it)
{
  return !((*this) == it);
}

std::map<LayerIterator::LayerRole, std::string> LayerIterator::m_RoleDefaultNames;


void LayerIterator::Print(const char *what) const
{
  std::cout << "LI with filter " << m_RoleFilter << " operation " << what << std::endl;
  if(this->IsAtEnd())
    {
    std::cout << "  AT END" << std::endl;
    }
  else
    {
    std::cout << "  Role:         " << m_RoleDefaultNames[this->GetRole()] << std::endl;
    std::cout << "  Pos. in Role: "
              << (int)(m_WrapperInRoleIter - m_RoleIter->second.begin()) << " of "
              << (int) m_RoleIter->second.size() << std::endl;
    std::cout << "  Valid:        " << this->IsPointingToListableLayer() << std::endl;
    }
}

