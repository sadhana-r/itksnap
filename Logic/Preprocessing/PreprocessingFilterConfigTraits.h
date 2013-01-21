/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: IRISApplication.cxx,v $
  Language:  C++
  Date:      $Date: 2011/04/18 17:35:30 $
  Version:   $Revision: 1.37 $
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

#ifndef PREPROCESSINGFILTERCONFIGTRAITS_H
#define PREPROCESSINGFILTERCONFIGTRAITS_H

#include <SNAPImageData.h>
template <class TInput, class TOutput> class SmoothBinaryThresholdImageFilter;
template <class TInput, class TOutput> class EdgePreprocessingImageFilter;
class ThresholdSettings;
class EdgePreprocessingSettings;

class SmoothBinaryThresholdFilterConfigTraits {
public:

  typedef ScalarImageWrapperBase::CommonFormatImageType               GreyType;
  typedef SNAPImageData::SpeedImageType                              SpeedType;
  typedef SpeedImageWrapper                                  OutputWrapperType;

  typedef SmoothBinaryThresholdImageFilter<GreyType, SpeedType>     FilterType;
  typedef ThresholdSettings                                      ParameterType;

  static void AttachInputs(SNAPImageData *sid, FilterType *filter, int channel);
  static void DetachInputs(FilterType *filter);
  static void SetParameters(ParameterType *p, FilterType *filter);
};

class EdgePreprocessingFilterConfigTraits {
public:

  typedef ScalarImageWrapperBase::CommonFormatImageType               GreyType;
  typedef SNAPImageData::SpeedImageType                              SpeedType;
  typedef SpeedImageWrapper                                  OutputWrapperType;

  typedef EdgePreprocessingImageFilter<GreyType, SpeedType>         FilterType;
  typedef EdgePreprocessingSettings                              ParameterType;

  static void AttachInputs(SNAPImageData *sid, FilterType *filter, int channel);
  static void DetachInputs(FilterType *filter);
  static void SetParameters(ParameterType *p, FilterType *filter);
};

#endif // PREPROCESSINGFILTERCONFIGTRAITS_H
