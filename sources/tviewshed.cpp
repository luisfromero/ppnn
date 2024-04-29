/******************************************************************************
*
* Project:  Total Viewshed Generation
* Purpose:  Core algorithm implementation for total viewshed generation.
* Author:   Luis Felipe Romero, felipe@uma.es , Felipe Romero, fr@uma.es,
* Author:   Tamas Szekeres, szekerest@gmail.com
*
******************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
****************************************************************************/

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cmath>
#include <cstring>
#include <array>
#include <limits>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "ogr_api.h"
#include "ogr_spatialref.h"
#include "ogr_core.h"
#include "commonutils.h"

CPL_CVSID("$Id$")

#include <windows.h>

#include <omp.h>
#include <CL/cl.hpp>
#include <iostream>

#include "tvs/tviewshed.h"




/************************************************************************/
/*                        GDALTViewshedGenerate()                         */
/************************************************************************/

/**
* Create viewshed from raster DEM.
*
* This algorithm will generate a viewshed raster from an input DEM raster
* by using a modified algorithm of "Generating Viewsheds without Using
* Sightlines" published at
* https://www.asprs.org/wp-content/uploads/pers/2000journal/january/2000_jan_87-90.pdf
* This appoach provides a relatively fast calculation, since the output raster
* is generated in a single scan. The gdal/apps/gdal_viewshed.cpp mainline can
* be used as an example of how to use this function. The output raster will be
* of type Byte or Float64.
*
* \note The algorithm as implemented currently will only output meaningful
* results if the georeferencing is in a projected coordinate reference system.
*
* @param hBand The band to read the DEM data from. Only the part of the raster
* within the specified maxdistance around the observer point is processed.
*
* @param pszDriverName Driver name (GTiff if set to NULL)
*
* @param pszTargetRasterName The name of the target raster to be generated.
* Must not be NULL
*
* @param papszCreationOptions creation options.
*
* @param dfObserverHeight The height of the observer above the DEM surface.
*
* @param dfMaxDistance maximum distance range to compute viewshed.
*                      It is also used to clamp the extent of the output
* raster. If set to 0, then unlimited range is assumed, that is to say the
*                      computation is performed on the extent of the whole
* raster.
*
* @param pfnProgress A GDALProgressFunc that may be used to report progress
* to the user, or to interrupt the algorithm.  May be NULL if not required.
*
* @param pProgressArg The callback data for the pfnProgress function.
*
* @param heightMode Type of information contained in output raster. Possible
* values GVOT_NORMAL = 1 (default), GVOT_MIN_TARGET_HEIGHT_FROM_DEM = 2,
*                   GVOT_MIN_TARGET_HEIGHT_FROM_GROUND = 3
*
*                   GVOT_NORMAL returns a raster of type Byte containing
* visible locations.
*
*                   GVOT_MIN_TARGET_HEIGHT_FROM_DEM and
* GVOT_MIN_TARGET_HEIGHT_FROM_GROUND will return a raster of type Float64
* containing the minimum target height for target to be visible from the DEM
* surface or ground level respectively. Parameters dfTargetHeight, dfVisibleVal
* and dfInvisibleVal will be ignored.
*
*
* @param papszExtraOptions Future extra options. Must be set to NULL currently.
*
* @return not NULL output dataset on success (to be closed with GDALClose()) or
* NULL if an error occurs.
*
* @since GDAL 3.1
*/

GDALDatasetH GDALTViewshedGenerate(
   GDALRasterBandH hBand, const char *pszDriverName,
   const char *pszTargetRasterName, CSLConstList papszCreationOptions,
   double dfObserverX, double dfObserverY, double dfObserverHeight,
   double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
   double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
   GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
   void *pProgressArg, GDALTViewshedOutputType outputType_p,
   CSLConstList papszExtraOptions, GDALRasterBandH maskBand)

{
   VALIDATE_POINTER1(hBand, "GDALTViewshedGenerate", nullptr);
   VALIDATE_POINTER1(pszTargetRasterName, "GDALTViewshedGenerate", nullptr);


   runMode=HYBRID_MODE;
   //runMode=CPU_MODE;
   gpuMode=OPENCL_MODE;
   outputType=outputType_p;
   setResources(); // Create cpu and gpu interfaces, set nCPUs, nGPUs
   int nXSize = GDALGetRasterBandXSize(hBand);
   int nYSize = GDALGetRasterBandYSize(hBand);
   int pbSuccess;
   double bandStep = GDALGetRasterScale(hBand, nullptr);
   /* set up geotransformation */
   std::array<double, 6> adfGeoTransform{{0.0, 1.0, 0.0, 0.0, 0.0, 1.0}};
   GDALDatasetH hSrcDS = GDALGetBandDataset(hBand);
   GDALDatasetH mSrcDS = maskBand==nullptr?nullptr:GDALGetBandDataset(maskBand);
   if (hSrcDS != nullptr)
       GDALGetGeoTransform(hSrcDS, adfGeoTransform.data());

   double adfInvGeoTransform[6];
   if (!GDALInvGeoTransform(adfGeoTransform.data(), adfInvGeoTransform))
   {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
       return nullptr;
   }



   /* normalize horizontal index (0 - nXSize) */


   double xStep=adfGeoTransform[1];
   double yStep=adfGeoTransform[5];
   printf("Size and scale = %d x %d, %e (w-e) x %e (n-s)\n",nXSize,nYSize,xStep,yStep);
   step=(abs(xStep)+abs(yStep))/2.0f;
   double errorX=abs(1.0-step/xStep);
   double errorY=abs(1.0-step/xStep);

   //if(errorX>0.01)
       printf("Error de malla %e  %e\n",errorX,errorY);

   CPL_IGNORE_RET_VAL(papszExtraOptions);

#define TVS

   const GByte byNoDataVal = dfNoDataVal >= 0 && dfNoDataVal <= 255
                                 ? static_cast<GByte>(dfNoDataVal)
                                 : 0;
#ifndef TVS
   const GByte byVisibleVal = dfVisibleVal >= 0 && dfVisibleVal <= 255
                                  ? static_cast<GByte>(dfVisibleVal)
                                  : 255;
   const GByte byInvisibleVal = dfInvisibleVal >= 0 && dfInvisibleVal <= 255
                                    ? static_cast<GByte>(dfInvisibleVal)
                                    : 0;
   const GByte byOutOfRangeVal = dfOutOfRangeVal >= 0 && dfOutOfRangeVal <= 255
                                     ? static_cast<GByte>(dfOutOfRangeVal)
                                     : 0;

   if (heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_DEM &&
       heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_GROUND)
       heightMode = GVOT_NORMAL;
#endif


#ifndef TVS
   /* calculate observer position */
   double dfX, dfY;
   GDALApplyGeoTransform(adfInvGeoTransform, dfObserverX, dfObserverY, &dfX,
                         &dfY);
   int nX = static_cast<int>(dfX);
   int nY = static_cast<int>(dfY);
#endif
#ifndef TVS
   if (nX < 0 || nX > nXSize || nY < 0 || nY > nYSize)
   {
       CPLError(CE_Failure, CPLE_AppDefined,
                "The observer location falls outside of the DEM area");
       return nullptr;
   }

   /* calculate the area of interest */
   int nXStart =
       dfMaxDistance > 0
           ? (std::max)(0, static_cast<int>(std::floor(
                               nX - adfInvGeoTransform[1] * dfMaxDistance)))
           : 0;
   int nXStop =
       dfMaxDistance > 0
           ? (std::min)(nXSize,
                        static_cast<int>(std::ceil(nX + adfInvGeoTransform[1] *
                                                            dfMaxDistance) +
                                         1))
           : nXSize;
   int nYStart =
       dfMaxDistance > 0
           ? (std::max)(0, static_cast<int>(std::floor(
                               nY + adfInvGeoTransform[5] * dfMaxDistance)))
           : 0;
   int nYStop =
       dfMaxDistance > 0
           ? (std::min)(nYSize,
                        static_cast<int>(std::ceil(nY - adfInvGeoTransform[5] *
                                                            dfMaxDistance) +
                                         1))
           : nYSize;

   /* normalize horizontal index (0 - nXSize) */
   nXSize = nXStop - nXStart;
   nX -= nXStart;

   nYSize = nYStop - nYStart;

   if (nXSize == 0 || nYSize == 0)
   {
       CPLError(CE_Failure, CPLE_AppDefined, "Invalid target raster size");
       return nullptr;
   }
#else

   int nXStart=0;
   int nXStop=nXSize;
   int nYStart=0;
   int nYStop=nYSize;

#endif
#ifndef TVS

   std::vector<double> vFirstLineVal;
   std::vector<double> vLastLineVal;
   std::vector<double> vThisLineVal;
   std::vector<GByte> vResult;
   std::vector<double> vHeightResult;

   try
   {
       vFirstLineVal.resize(nXSize);
       vLastLineVal.resize(nXSize);
       vThisLineVal.resize(nXSize);
       vResult.resize(nXSize);

       if (heightMode != GVOT_NORMAL)
           vHeightResult.resize(nXSize);
   }
   catch (...)
   {
       CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot allocate vectors for viewshed");
       return nullptr;
   }

   double *padfFirstLineVal = vFirstLineVal.data();
   double *padfLastLineVal = vLastLineVal.data();
   double *padfThisLineVal = vThisLineVal.data();
   GByte *pabyResult = vResult.data();
   double *dfHeightResult = vHeightResult.data();
#endif

   GDALDriverManager *hMgr = GetGDALDriverManager();
   GDALDriver *hDriver =
       hMgr->GetDriverByName(pszDriverName ? pszDriverName : "GTiff");
   if (!hDriver)
   {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
       return nullptr;
   }

   /* create output raster */
#ifdef TVS

   bool isHorizon=outputType==2;
   auto poDstDS = std::unique_ptr<GDALDataset>(
       hDriver->Create(pszTargetRasterName, nXSize, nYSize,isHorizon?nSectors:1 ,
                       isHorizon?GDT_Byte:GDT_Float32,
                       const_cast<char **>(papszCreationOptions)));
#else
   auto poDstDS = std::unique_ptr<GDALDataset>(
       hDriver->Create(pszTargetRasterName, nXSize, nYStop - nYStart, 1,
                       heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
                       const_cast<char **>(papszCreationOptions)));
#endif
   if (!poDstDS)
   {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                pszTargetRasterName);
       return nullptr;
   }
   /* copy srs */
   if (hSrcDS)
       poDstDS->SetSpatialRef(
           GDALDataset::FromHandle(hSrcDS)->GetSpatialRef());

   std::array<double, 6> adfDstGeoTransform;
   adfDstGeoTransform[0] = adfGeoTransform[0] + adfGeoTransform[1] * nXStart +
                           adfGeoTransform[2] * nYStart;
   adfDstGeoTransform[1] = adfGeoTransform[1];
   adfDstGeoTransform[2] = adfGeoTransform[2];
   adfDstGeoTransform[3] = adfGeoTransform[3] + adfGeoTransform[4] * nXStart +
                           adfGeoTransform[5] * nYStart;
   adfDstGeoTransform[4] = adfGeoTransform[4];
   adfDstGeoTransform[5] = adfGeoTransform[5];
   poDstDS->SetGeoTransform(adfDstGeoTransform.data());

   auto hTargetBand = poDstDS->GetRasterBand(1);
   if (hTargetBand == nullptr)
   {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                pszTargetRasterName);
       return nullptr;
   }
#ifdef TVS
   if (dfNoDataVal >= 0)
       GDALSetRasterNoDataValue(
           hTargetBand, dfNoDataVal);
#else
   if (dfNoDataVal >= 0)
       GDALSetRasterNoDataValue(
           hTargetBand, heightMode != GVOT_NORMAL ? dfNoDataVal : byNoDataVal);



/* process first line */
   if (GDALRasterIO(hBand, GF_Read, nXStart, nY, nXSize, 1, padfFirstLineVal,
                    nXSize, 1, GDT_Float64, 0, 0))
   {
       CPLError(
           CE_Failure, CPLE_AppDefined,
           "RasterIO error when reading DEM at position(%d, %d), size(%d, %d)",
           nXStart, nY, nXSize, 1);
       return nullptr;
   }

   const double dfZObserver = dfObserverHeight + padfFirstLineVal[nX];
   double dfZ = 0.0;
   const double dfDistance2 = dfMaxDistance * dfMaxDistance;
#endif
   /* If we can't get a SemiMajor axis from the SRS, it will be
    * SRS_WGS84_SEMIMAJOR
    */
   double dfSphereDiameter(std::numeric_limits<double>::infinity());
   const OGRSpatialReference *poDstSRS = poDstDS->GetSpatialRef();
   if (poDstSRS)
   {
       OGRErr eSRSerr;
       double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

       /* If we fetched the axis from the SRS, use it */
       if (eSRSerr != OGRERR_FAILURE)
           dfSphereDiameter = dfSemiMajor * 2.0;
       else
           CPLDebug("GDALTViewshedGenerate",
                    "Unable to fetch SemiMajor axis from spatial reference");
   }
#ifndef TVS
   /* mark the observer point as visible */
   double dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                              ? padfFirstLineVal[nX]
                              : 0.0;
   pabyResult[nX] = byVisibleVal;
   if (heightMode != GVOT_NORMAL)
       dfHeightResult[nX] = dfGroundLevel;

   // Felipe. Anular tvieshed
   if (true)
   {

       if (nX > 0)
       {
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfFirstLineVal[nX - 1]
                               : 0.0;
           CPL_IGNORE_RET_VAL(AdjustHeightInRange(
               adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX - 1], dfDistance2,
               dfCurvCoeff, dfSphereDiameter));
           pabyResult[nX - 1] = byVisibleVal;
           if (heightMode != GVOT_NORMAL)
               dfHeightResult[nX - 1] = dfGroundLevel;
       }
       if (nX < nXSize - 1)
       {
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfFirstLineVal[nX + 1]
                               : 0.0;
           CPL_IGNORE_RET_VAL(AdjustHeightInRange(
               adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX + 1], dfDistance2,
               dfCurvCoeff, dfSphereDiameter));
           pabyResult[nX + 1] = byVisibleVal;
           if (heightMode != GVOT_NORMAL)
               dfHeightResult[nX + 1] = dfGroundLevel;
       }

       /* process left direction */
       for (int iPixel = nX - 2; iPixel >= 0; iPixel--)
       {
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfFirstLineVal[iPixel]
                               : 0.0;
           bool adjusted = AdjustHeightInRange(
               adfGeoTransform.data(), nX - iPixel, 0, padfFirstLineVal[iPixel],
               dfDistance2, dfCurvCoeff, dfSphereDiameter);
           if (adjusted)
           {
               dfZ = CalcHeightLine(nX - iPixel, padfFirstLineVal[iPixel + 1],
                                    dfZObserver);

               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[iPixel] = std::max(
                       0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

               SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                             vResult, byVisibleVal, byInvisibleVal);
           }
           else
           {
               for (; iPixel >= 0; iPixel--)
               {
                   pabyResult[iPixel] = byOutOfRangeVal;
                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = dfOutOfRangeVal;
               }
           }
       }
       /* process right direction */
       for (int iPixel = nX + 2; iPixel < nXSize; iPixel++)
       {
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfFirstLineVal[iPixel]
                               : 0.0;
           bool adjusted = AdjustHeightInRange(
               adfGeoTransform.data(), iPixel - nX, 0, padfFirstLineVal[iPixel],
               dfDistance2, dfCurvCoeff, dfSphereDiameter);
           if (adjusted)
           {
               dfZ = CalcHeightLine(iPixel - nX, padfFirstLineVal[iPixel - 1],
                                    dfZObserver);

               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[iPixel] = std::max(
                       0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

               SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                             vResult, byVisibleVal, byInvisibleVal);
           }
           else
           {
               for (; iPixel < nXSize; iPixel++)
               {
                   pabyResult[iPixel] = byOutOfRangeVal;
                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = dfOutOfRangeVal;
               }
           }
       }
       /* write result line */

       if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - nYStart, nXSize, 1,
                        heightMode != GVOT_NORMAL
                            ? static_cast<void *>(dfHeightResult)
                            : static_cast<void *>(pabyResult),
                        nXSize, 1,
                        heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte, 0, 0))
       {
           CPLError(CE_Failure, CPLE_AppDefined,
                    "RasterIO error when writing target raster at position "
                    "(%d,%d), size (%d,%d)",
                    0, nY - nYStart, nXSize, 1);
           return nullptr;
       }



       /* scan upwards */
       std::copy(vFirstLineVal.begin(), vFirstLineVal.end(), vLastLineVal.begin());
       for (int iLine = nY - 1; iLine >= nYStart; iLine--)
       {
           if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
                            padfThisLineVal, nXSize, 1, GDT_Float64, 0, 0))
           {
               CPLError(CE_Failure, CPLE_AppDefined,
                        "RasterIO error when reading DEM at position (%d,%d), "
                        "size (%d,%d)",
                        nXStart, iLine, nXSize, 1);
               return nullptr;
           }

           /* set up initial point on the scanline */
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfThisLineVal[nX]
                               : 0.0;
           bool adjusted = AdjustHeightInRange(
               adfGeoTransform.data(), 0, nY - iLine, padfThisLineVal[nX],
               dfDistance2, dfCurvCoeff, dfSphereDiameter);
           if (adjusted)
           {
               dfZ = CalcHeightLine(nY - iLine, padfLastLineVal[nX], dfZObserver);

               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[nX] =
                       std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

               SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                             byVisibleVal, byInvisibleVal);
           }
           else
           {
               pabyResult[nX] = byOutOfRangeVal;
               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[nX] = dfOutOfRangeVal;
           }

           /* process left direction */
           for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
           {
               dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                                   ? padfThisLineVal[iPixel]
                                   : 0.0;
               bool left_adjusted =
                   AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                                       nY - iLine, padfThisLineVal[iPixel],
                                       dfDistance2, dfCurvCoeff, dfSphereDiameter);
               if (left_adjusted)
               {
                   if (eMode != GVM_Edge)
                       dfZ = CalcHeightDiagonal(
                           nX - iPixel, nY - iLine, padfThisLineVal[iPixel + 1],
                           padfLastLineVal[iPixel], dfZObserver);

                   if (eMode != GVM_Diagonal)
                   {
                       double dfZ2 =
                           nX - iPixel >= nY - iLine
                               ? CalcHeightEdge(nY - iLine, nX - iPixel,
                                                padfLastLineVal[iPixel + 1],
                                                padfThisLineVal[iPixel + 1],
                                                dfZObserver)
                               : CalcHeightEdge(nX - iPixel, nY - iLine,
                                                padfLastLineVal[iPixel + 1],
                                                padfLastLineVal[iPixel],
                                                dfZObserver);
                       dfZ = CalcHeight(dfZ, dfZ2, eMode);
                   }

                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = std::max(
                           0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                   SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                                 vResult, byVisibleVal, byInvisibleVal);
               }
               else
               {
                   for (; iPixel >= 0; iPixel--)
                   {
                       pabyResult[iPixel] = byOutOfRangeVal;
                       if (heightMode != GVOT_NORMAL)
                           dfHeightResult[iPixel] = dfOutOfRangeVal;
                   }
               }
           }
           /* process right direction */
           for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
           {
               dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                                   ? padfThisLineVal[iPixel]
                                   : 0.0;
               bool right_adjusted =
                   AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                                       nY - iLine, padfThisLineVal[iPixel],
                                       dfDistance2, dfCurvCoeff, dfSphereDiameter);
               if (right_adjusted)
               {
                   if (eMode != GVM_Edge)
                       dfZ = CalcHeightDiagonal(
                           iPixel - nX, nY - iLine, padfThisLineVal[iPixel - 1],
                           padfLastLineVal[iPixel], dfZObserver);

                   if (eMode != GVM_Diagonal)
                   {
                       double dfZ2 =
                           iPixel - nX >= nY - iLine
                               ? CalcHeightEdge(nY - iLine, iPixel - nX,
                                                padfLastLineVal[iPixel - 1],
                                                padfThisLineVal[iPixel - 1],
                                                dfZObserver)
                               : CalcHeightEdge(iPixel - nX, nY - iLine,
                                                padfLastLineVal[iPixel - 1],
                                                padfLastLineVal[iPixel],
                                                dfZObserver);
                       dfZ = CalcHeight(dfZ, dfZ2, eMode);
                   }

                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = std::max(
                           0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                   SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                                 vResult, byVisibleVal, byInvisibleVal);
               }
               else
               {
                   for (; iPixel < nXSize; iPixel++)
                   {
                       pabyResult[iPixel] = byOutOfRangeVal;
                       if (heightMode != GVOT_NORMAL)
                           dfHeightResult[iPixel] = dfOutOfRangeVal;
                   }
               }
           }

           /* write result line */
           if (GDALRasterIO(
                   hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                   heightMode != GVOT_NORMAL ? static_cast<void *>(dfHeightResult)
                                             : static_cast<void *>(pabyResult),
                   nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
                   0, 0))
           {
               CPLError(CE_Failure, CPLE_AppDefined,
                        "RasterIO error when writing target raster at position "
                        "(%d,%d), size (%d,%d)",
                        0, iLine - nYStart, nXSize, 1);
               return nullptr;
           }

           std::swap(padfLastLineVal, padfThisLineVal);

           if (!pfnProgress((nY - iLine) / static_cast<double>(nYSize), "",
                            pProgressArg))
           {
               CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
               return nullptr;
           }
       }
       /* scan downwards */
       memcpy(padfLastLineVal, padfFirstLineVal, nXSize * sizeof(double));
       for (int iLine = nY + 1; iLine < nYStop; iLine++)
       {
           if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXStop - nXStart, 1,
                            padfThisLineVal, nXStop - nXStart, 1, GDT_Float64, 0,
                            0))
           {
               CPLError(CE_Failure, CPLE_AppDefined,
                        "RasterIO error when reading DEM at position (%d,%d), "
                        "size (%d,%d)",
                        nXStart, iLine, nXStop - nXStart, 1);
               return nullptr;
           }

           /* set up initial point on the scanline */
           dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                               ? padfThisLineVal[nX]
                               : 0.0;
           bool adjusted = AdjustHeightInRange(
               adfGeoTransform.data(), 0, iLine - nY, padfThisLineVal[nX],
               dfDistance2, dfCurvCoeff, dfSphereDiameter);
           if (adjusted)
           {
               dfZ = CalcHeightLine(iLine - nY, padfLastLineVal[nX], dfZObserver);

               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[nX] =
                       std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

               SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                             byVisibleVal, byInvisibleVal);
           }
           else
           {
               pabyResult[nX] = byOutOfRangeVal;
               if (heightMode != GVOT_NORMAL)
                   dfHeightResult[nX] = dfOutOfRangeVal;
           }

           /* process left direction */
           for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
           {
               dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                                   ? padfThisLineVal[iPixel]
                                   : 0.0;
               bool left_adjusted =
                   AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                                       iLine - nY, padfThisLineVal[iPixel],
                                       dfDistance2, dfCurvCoeff, dfSphereDiameter);
               if (left_adjusted)
               {
                   if (eMode != GVM_Edge)
                       dfZ = CalcHeightDiagonal(
                           nX - iPixel, iLine - nY, padfThisLineVal[iPixel + 1],
                           padfLastLineVal[iPixel], dfZObserver);

                   if (eMode != GVM_Diagonal)
                   {
                       double dfZ2 =
                           nX - iPixel >= iLine - nY
                               ? CalcHeightEdge(iLine - nY, nX - iPixel,
                                                padfLastLineVal[iPixel + 1],
                                                padfThisLineVal[iPixel + 1],
                                                dfZObserver)
                               : CalcHeightEdge(nX - iPixel, iLine - nY,
                                                padfLastLineVal[iPixel + 1],
                                                padfLastLineVal[iPixel],
                                                dfZObserver);
                       dfZ = CalcHeight(dfZ, dfZ2, eMode);
                   }

                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = std::max(
                           0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                   SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                                 vResult, byVisibleVal, byInvisibleVal);
               }
               else
               {
                   for (; iPixel >= 0; iPixel--)
                   {
                       pabyResult[iPixel] = byOutOfRangeVal;
                       if (heightMode != GVOT_NORMAL)
                           dfHeightResult[iPixel] = dfOutOfRangeVal;
                   }
               }
           }
           /* process right direction */
           for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
           {
               dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                                   ? padfThisLineVal[iPixel]
                                   : 0.0;
               bool right_adjusted =
                   AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                                       iLine - nY, padfThisLineVal[iPixel],
                                       dfDistance2, dfCurvCoeff, dfSphereDiameter);
               if (right_adjusted)
               {
                   if (eMode != GVM_Edge)
                       dfZ = CalcHeightDiagonal(
                           iPixel - nX, iLine - nY, padfThisLineVal[iPixel - 1],
                           padfLastLineVal[iPixel], dfZObserver);

                   if (eMode != GVM_Diagonal)
                   {
                       double dfZ2 =
                           iPixel - nX >= iLine - nY
                               ? CalcHeightEdge(iLine - nY, iPixel - nX,
                                                padfLastLineVal[iPixel - 1],
                                                padfThisLineVal[iPixel - 1],
                                                dfZObserver)
                               : CalcHeightEdge(iPixel - nX, iLine - nY,
                                                padfLastLineVal[iPixel - 1],
                                                padfLastLineVal[iPixel],
                                                dfZObserver);
                       dfZ = CalcHeight(dfZ, dfZ2, eMode);
                   }

                   if (heightMode != GVOT_NORMAL)
                       dfHeightResult[iPixel] = std::max(
                           0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                   SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                                 vResult, byVisibleVal, byInvisibleVal);
               }
               else
               {
                   for (; iPixel < nXSize; iPixel++)
                   {
                       pabyResult[iPixel] = byOutOfRangeVal;
                       if (heightMode != GVOT_NORMAL)
                           dfHeightResult[iPixel] = dfOutOfRangeVal;
                   }
               }
           }

           /* write result line */
           if (GDALRasterIO(
                   hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                   heightMode != GVOT_NORMAL ? static_cast<void *>(dfHeightResult)
                                             : static_cast<void *>(pabyResult),
                   nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
                   0, 0))
           {
               CPLError(CE_Failure, CPLE_AppDefined,
                        "RasterIO error when writing target raster at position "
                        "(%d,%d), size (%d,%d)",
                        0, iLine - nYStart, nXSize, 1);
               return nullptr;
           }

           std::swap(padfLastLineVal, padfThisLineVal);

           if (!pfnProgress((iLine - nYStart) / static_cast<double>(nYSize), "",
                            pProgressArg))
           {
               CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
               return nullptr;
           }
       }
   }
   // Felipe anular point vs

   if (!pfnProgress(1.0, "", pProgressArg))
   {
       CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
       return nullptr;
   }
#endif

   surScale=abs(M_PI/(360*xStep*yStep));
   POVh=(float)dfObserverHeight/step;
   maxDistance=(float)dfMaxDistance;
   dimx=nXSize;
   dimy=nYSize;
   N=dim=dimx*dimy;
   float * inputD=(float *)malloc(nXSize*nYSize*sizeof(float));
   if (!GDALRasterIO(hBand, GF_Read, nXStart, nYStart, nXSize, nYSize, inputD,nXSize, nYSize, GDT_Float32, 0, 0))
   {
       //float * salida;//=(float *)malloc(nXSize*nYSize*sizeof(float));
       if(maskBand!=nullptr){
           maskPOVs=1;
           float * inputM=(float *)malloc(nXSize*nYSize*sizeof(float));
           if (GDALRasterIO(maskBand, GF_Read, nXStart, nYStart, nXSize, nYSize, inputM,nXSize, nYSize, GDT_Float32, 0, 0))
           {
               maskPOVs=0;
           }
           else
           {
               //The mask is encoded in the least significant bit of height mantissa;
               void *y;
               int cnt=0;
               for(int i=0;i<nYSize*nXSize;i++){
                   int *x= static_cast<int *>(y = &inputD[i]);
                   bool masked=inputM[i]>0;
                   if(masked)cnt++;
                   *x= masked? ((int ) *x) |1 : ((int ) *x) &0xFFFFFFFE;
               }
               printf("Singular viewshed computation for %d points\n",cnt);
           }
           free(inputM);
       }
       //Allocate es static. El objeto aún no existe
       //pfnProgress(0, "", pProgressArg);

       if (pfnProgress == nullptr)
           pfnProgress = GDALDummyProgress;

       if (!pfnProgress(0.0, "", pProgressArg))
       {
           CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
           return nullptr;
       }


       int dataSize=sizeof(float);
       inData.input0=(float*)malloc(dim*dataSize);
       inData.input1=(float*)malloc(dim*dataSize);
       inData.input2=(float*)malloc(dim*dataSize);
       inData.input3=(float*)malloc(dim*dataSize);
       //memcpy(inData.input0,inputD,dim*dataSize); //Move input data to pinned memory

       for(int i=0;i<dim;i++)inData.input0[i]=inputD[i]/step;
       //from now on, step is the unit
       free(inputD);

       if(isHorizon)
       {
           for(int i=0;i<360;i++){
               horizontes[i]=(unsigned char *)malloc(dim);
               if(horizontes[i]==nullptr)
                   exit(0); //ToDo
           }
       }
       else
       {
           outD=(float *)malloc(dim*dataSize);
           memset(outD,0,dim*dataSize);
       }
       inData=  skewEngine<float>::prepare(&inData,dimx,dimy);// Rotated and mirror
       pair_t mmi = getMinMax(inData.input0);
       execute(pfnProgress,pProgressArg);
       if(outputType!=TVS_HORIZON){
           pair_t mm = getMinMax(outD);
           if(verbose)
           {
               printf("Extreme values (unscaled) for output: %6.2f | %e  \n ", mm.min, mm.max);
               printf("Extreme values for output: %6.2f | %e  (scale = %f )\n ", mm.min * surScale, mm.max * surScale, surScale);
               fflush(stdout);
           }
           GDALDataType dt=GDT_Float32 ;
           GDALRasterIO(hTargetBand, GF_Write, 0, 0, nXSize, nYSize,outD,nXSize, nYSize,dt,0, 0);
           free(outD);
       }
       else
       {
           // ToDo ¿interlaced?¿multiband?¿database?¿zip?
           GDALDataType dt=GDT_Byte ;
           GDALRasterIO(hTargetBand, GF_Write, 0, 0, nXSize, nYSize,outH,nXSize, nYSize,GDT_Byte,0, 0);
           for(int i=0;i<360;i++)free(horizontes[i]);
       }

       return GDALDataset::FromHandle(poDstDS.release());
   }

   return nullptr;
}
