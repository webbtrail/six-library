/* =========================================================================
 * This file is part of six-c++ 
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2014, MDA Information Systems LLC
 *
 * six-c++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */
#include <iostream>

#include <import/cli.h>
#include <import/six.h>
#include <import/io.h>
#include <logging/Setup.h>
#include <scene/Utilities.h>
#include "utils.h"

// For SICD implementation
#include <import/six/sicd.h>
#include <six/sicd/SICDWriteControl.h>

namespace
{
class Compare
{
public:
    Compare(const six::Data& lhsData,
            const std::vector<std::complex<float> >& lhsImage,
            const std::vector<std::string>& schemaPaths) :
        mLhsData(*reinterpret_cast<const six::sicd::ComplexData*>(lhsData.clone())),
        mLhsImage(lhsImage),
        mSchemaPaths(schemaPaths)
    {
    }

    bool operator()(const std::string& pathname) const
    {
        std::auto_ptr<six::sicd::ComplexData> rhsData;
        std::vector<std::complex<float> > rhsImage;
        six::sicd::Utilities::readSicd(pathname,
                                       mSchemaPaths,
                                       rhsData,
                                       rhsImage);

        if (mLhsImage != rhsImage)
        {
            for (size_t ii = 0; ii < mLhsImage.size(); ++ii)
            {
                if (mLhsImage[ii] != rhsImage[ii])
                {
                    std::cout << "Stops matching at " << ii << std::endl;
                    break;
                }
            }
        }

        return (mLhsData == *rhsData && mLhsImage == rhsImage);
    }

private:
    const six::sicd::ComplexData mLhsData;
    const std::vector<std::complex<float> > mLhsImage;
    const std::vector<std::string> mSchemaPaths;
};

template <typename T>
void subsetData(const std::vector<T>& orig,
                size_t origNumCols,
                const types::RowCol<size_t>& offset,
                const types::RowCol<size_t>& dims,
                std::vector<T>& output)
{
    output.resize(dims.area());
    const T* origPtr = &orig[0] + offset.row * origNumCols + offset.col;
    T* outputPtr = &output[0];
    for (size_t row = 0;
         row < dims.row;
         ++row, origPtr += origNumCols, outputPtr += dims.col)
    {
        ::memcpy(outputPtr, origPtr, dims.col * sizeof(T));
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        // create a parser and add our options to it
        cli::ArgumentParser parser;
        parser.setDescription(
                              "This program creates a sample SICD NITF file of all zeros.");
        parser.addArgument("-r --rows", "Rows limit", cli::STORE, "maxRows",
                           "ROWS")->setDefault(-1);
        parser.addArgument("-s --size", "Max product size", cli::STORE,
                           "maxSize", "BYTES")->setDefault(-1);
        parser.addArgument("--class", "Classification Level", cli::STORE,
                           "classLevel", "LEVEL")->setDefault("UNCLASSIFIED");
        parser.addArgument("--schema", 
                           "Specify a schema or directory of schemas",
                           cli::STORE);
        parser.addArgument("output", "Output filename", cli::STORE, "output",
                           "OUTPUT", 1, 1);

        std::auto_ptr<cli::Results> options(parser.parse(argc, argv));

        std::string outputName(options->get<std::string> ("output"));
        size_t maxRows(options->get<size_t>("maxRows"));
        size_t maxSize(options->get<size_t>("maxSize"));
        std::string classLevel(options->get<std::string> ("classLevel"));
        std::vector<std::string> schemaPaths;
        getSchemaPaths(*options, "--schema", "schema", schemaPaths);

        std::auto_ptr<logging::Logger> logger(
                logging::setupLogger(sys::Path::basename(argv[0])));

        // create an XML registry
        // The reason to do this is to avoid adding XMLControlCreators to the
        // XMLControlFactory singleton - this way has more fine-grained control
        //        XMLControlRegistry xmlRegistry;
        //        xmlRegistry.addCreator(DataType::COMPLEX, new XMLControlCreatorT<
        //                six::sicd::ComplexXMLControl> ());

        six::XMLControlFactory::getInstance().addCreator(
                six::DataType::COMPLEX,
                new six::XMLControlCreatorT<six::sicd::ComplexXMLControl>());

        // TODO: Allow this size to be overridden
        const types::RowCol<size_t> dims(123, 456);
        std::vector<std::complex<float> > image(dims.row * dims.col);
        for (size_t ii = 0; ii < image.size(); ++ii)
        {
            const float value = static_cast<float>(ii);
            image[ii] = std::complex<float>(value, value);
        }
        std::complex<float>* const imagePtr = &image[0];

        // Create the Data
        // TODO: Use a ComplexDataBuilder?
        six::sicd::ComplexData* data(new six::sicd::ComplexData());
        std::auto_ptr<six::Data> scopedData(data);
        data->setPixelType(six::PixelType::RE32F_IM32F);
        data->setNumRows(dims.row);
        data->setNumCols(dims.col);
        data->setName("corename");
        data->setSource("sensorname");
        data->collectionInformation->classification.level = classLevel;
        data->setCreationTime(six::DateTime());
        data->setImageCorners(makeUpCornersFromDMS());
        data->collectionInformation->radarMode = six::RadarModeType::SPOTLIGHT;
        data->scpcoa->sideOfTrack = six::SideOfTrackType::LEFT;
        data->geoData->scp.llh = six::LatLonAlt(42.2708, -83.7264);
        data->geoData->scp.ecf =
                scene::Utilities::latLonToECEF(data->geoData->scp.llh);
        data->grid->timeCOAPoly = six::Poly2D(0, 0);
        data->grid->timeCOAPoly[0][0] = 15605743.142846;
        data->position->arpPoly = six::PolyXYZ(0);
        data->position->arpPoly[0] = 0.0;

        data->radarCollection->txFrequencyMin = 0.0;
        data->radarCollection->txFrequencyMax = 0.0;
        data->radarCollection->txPolarization = six::PolarizationType::OTHER;
        mem::ScopedCloneablePtr<six::sicd::ChannelParameters>
                rcvChannel(new six::sicd::ChannelParameters());
        rcvChannel->txRcvPolarization = six::DualPolarizationType::OTHER;
        data->radarCollection->rcvChannels.push_back(rcvChannel);

        data->grid->row->sign = six::FFTSign::POS;
        data->grid->row->unitVector = 0.0;
        data->grid->row->sampleSpacing = 0;
        data->grid->row->impulseResponseWidth = 0;
        data->grid->row->impulseResponseBandwidth = 0;
        data->grid->row->kCenter = 0;
        data->grid->row->deltaK1 = 0;
        data->grid->row->deltaK2 = 0;
        data->grid->col->sign = six::FFTSign::POS;
        data->grid->col->unitVector = 0.0;
        data->grid->col->sampleSpacing = 0;
        data->grid->col->impulseResponseWidth = 0;
        data->grid->col->impulseResponseBandwidth = 0;
        data->grid->col->kCenter = 0;
        data->grid->col->deltaK1 = 0;
        data->grid->col->deltaK2 = 0;

        data->imageFormation->rcvChannelProcessed->numChannelsProcessed = 1;
        data->imageFormation->rcvChannelProcessed->channelIndex.push_back(0);

        data->pfa.reset(new six::sicd::PFA());
        data->pfa->spatialFrequencyScaleFactorPoly = six::Poly1D(0);
        data->pfa->spatialFrequencyScaleFactorPoly[0] = 42;
        data->pfa->polarAnglePoly = six::Poly1D(0);
        data->pfa->polarAnglePoly[0] = 42;

        data->timeline->collectDuration = 0;
        data->imageFormation->txRcvPolarizationProc =
                six::DualPolarizationType::OTHER;
        data->imageFormation->tStartProc = 0;
        data->imageFormation->tEndProc = 0;

        data->scpcoa->scpTime = 15605743.142846;
        data->scpcoa->slantRange = 0.0;
        data->scpcoa->groundRange = 0.0;
        data->scpcoa->dopplerConeAngle = 0.0;
        data->scpcoa->grazeAngle = 0.0;
        data->scpcoa->incidenceAngle = 0.0;
        data->scpcoa->twistAngle = 0.0;
        data->scpcoa->slopeAngle = 0.0;
        data->scpcoa->azimAngle = 0.0;
        data->scpcoa->layoverAngle = 0.0;
        data->scpcoa->arpPos = 0.0;
        data->scpcoa->arpVel = 0.0;
        data->scpcoa->arpAcc = 0.0;

        data->pfa->focusPlaneNormal = 0.0;
        data->pfa->imagePlaneNormal = 0.0;
        data->pfa->polarAngleRefTime = 0.0;
        data->pfa->krg1 = 0;
        data->pfa->krg2 = 0;
        data->pfa->kaz1 = 0;
        data->pfa->kaz2 = 0;

        data->imageFormation->txFrequencyProcMin = 0;
        data->imageFormation->txFrequencyProcMax = 0;

        six::Container container(six::DataType::COMPLEX);
        container.addData(scopedData);
        six::NITFWriteControl writer;
        writer.setLogger(logger.get());

        /*
         *  Under normal circumstances, the library uses the
         *  segmentation algorithm in the SICD spec, and numRowsLimit
         *  is set to Contants::ILOC_SZ.  If the user sets this, they
         *  want us to create an alternate numRowsLimit to force the
         *  library to segment on smaller boundaries.
         *
         *  This is handy especially for debugging, since it will force
         *  the algorithm to segment early.
         *
         */
        std::cout << "TODO: Fix overrides\n";
        /*
        if (maxRows > 0)
        {
            std::cout << "Overriding NITF max ILOC" << std::endl;
            writer.getOptions().setParameter(six::NITFWriteControl::OPT_MAX_ILOC_ROWS,
                                             maxRows);

        }
        if (maxSize > 0)
        {
            std::cout << "Overriding NITF product size" << std::endl;
            writer.getOptions().setParameter(six::NITFWriteControl::OPT_MAX_PRODUCT_SIZE,
                                             maxSize);
        }
        */

        // Write the file out the regular way
        writer.initialize(&container);

        six::BufferList buffers;

        buffers.push_back(reinterpret_cast<six::UByte*>(imagePtr));
        writer.save(buffers, outputName, schemaPaths);

        // Write the file out with a SICDWriteControl in one shot
        std::string otherPathname = "foo_1.nitf";
        {
            six::sicd::SICDWriteControl sicdWriter(otherPathname,
                                                   schemaPaths);
            sicdWriter.initialize(&container);
            sicdWriter.save(imagePtr,
                            types::RowCol<size_t>(0, 0),
                            dims);
        }

        // Let's see if things match
        const Compare compare(*container.getData(0), image, schemaPaths);
        int retCode = 0;
        if (compare(otherPathname))
        {
            std::cout << "Match!\n";
        }
        else
        {
            retCode = 1;
            std::cerr << "NO MATCH!\n";
        }

        // Now let's try some writes with num cols == global num cols
        otherPathname = "foo_2.nitf";
        {
            six::sicd::SICDWriteControl sicdWriter(otherPathname,
                                                   schemaPaths);
            sicdWriter.initialize(&container);

            // Rows [40, 60)
            types::RowCol<size_t> offset(40, 0);
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(20, dims.col));

            // Rows [5, 25)
            offset.row = 5;
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(20, dims.col));

            // Rows [0, 5)
            offset.row = 0;
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(5, dims.col));

            // Rows [100, 123)
            offset.row = 100;
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(23, dims.col));

            // Rows [25, 40)
            offset.row = 25;
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(15, dims.col));

            // Rows [60, 100)
            offset.row = 60;
            sicdWriter.save(imagePtr + offset.row * dims.col,
                            offset,
                            types::RowCol<size_t>(40, dims.col));
        }

        if (compare(otherPathname))
        {
            std::cout << "Match!\n";
        }
        else
        {
            retCode = 1;
            std::cerr << "NO MATCH!\n";
        }

        // Now let's try with a partial number of columns
        otherPathname = "foo_3.nitf";
        {
            six::sicd::SICDWriteControl sicdWriter(otherPathname,
                                                   schemaPaths);
            sicdWriter.initialize(&container);

            // Rows [40, 60)
            // Cols [400, 456)
            types::RowCol<size_t> offset(40, 400);
            std::vector<std::complex<float> > subset;
            types::RowCol<size_t> subsetDims(20, 56);
            subsetData(image, dims.col, offset, subsetDims, subset);
            sicdWriter.save(&subset[0], offset, subsetDims);

            // Rows [60, 123)
            offset.row = 60;
            offset.col = 0;
            subsetDims.row = 63;
            subsetDims.col = dims.col;
            subsetData(image, dims.col, offset, subsetDims, subset);
            sicdWriter.save(&subset[0], offset, subsetDims);

            // Rows [40, 60)
            // Cols [150, 400)
            offset.row = 40;
            offset.col = 150;
            subsetDims.row = 20;
            subsetDims.col = 250;
            subsetData(image, dims.col, offset, subsetDims, subset);
            sicdWriter.save(&subset[0], offset, subsetDims);

            // Rows [0, 40)
            offset.row = 0;
            offset.col = 0;
            subsetDims.row = 40;
            subsetDims.col = dims.col;
            subsetData(image, dims.col, offset, subsetDims, subset);
            sicdWriter.save(&subset[0], offset, subsetDims);

            // Rows [40, 60)
            // Cols [0, 150)
            offset.row = 40;
            offset.col = 0;
            subsetDims.row = 20;
            subsetDims.col = 150;
            subsetData(image, dims.col, offset, subsetDims, subset);
            sicdWriter.save(&subset[0], offset, subsetDims);
        }

        if (compare(otherPathname))
        {
            std::cout << "Match!\n";
        }
        else
        {
            retCode = 1;
            std::cerr << "NO MATCH!\n";
        }

        // TODO: Test that NITF headers look right
        //       If force NITRO to set the file date/time, everything else will
        //       be identical and then can compare the file itself
        //       Test 16-bit writes
        //       Test multi-seg


        return retCode;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Caught std::exception: " << ex.what() << std::endl;
        return 1;
    }
    catch (const except::Exception& ex)
    {
        std::cerr << "Caught except::Exception: " << ex.getMessage()
                  << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Caught unknown exception\n";
        return 1;
    }
}
