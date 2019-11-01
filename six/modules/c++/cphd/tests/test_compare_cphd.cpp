/* =========================================================================
 * This file is part of cphd-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2019, MDA Information Systems LLC
 *
 * cphd-c++ is free software; you can redistribute it and/or modify
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
#include <fstream>
#include <memory>
#include <logging/NullLogger.h>
#include <cli/ArgumentParser.h>
#include <io/TempFile.h>
#include <io/FileInputStream.h>
#include <io/FileOutputStream.h>
#include <cphd/Metadata.h>
#include <cphd/PVPBlock.h>
#include <cphd/CPHDReader.h>
#include <str/Convert.h>

/*!
 * Compares two CPHD files
 * Returns true if they do, false if not
 */

template <typename T>
bool compareCPHDData(const sys::ubyte* data1,
                     const sys::ubyte* data2,
                     size_t size,
                     size_t channel)
{
    const T* castData1 = reinterpret_cast<const T*>(data1);
    const T* castData2 = reinterpret_cast<const T*>(data2);

    for (size_t ii = 0; ii < size; ++ii)
    {
        if (castData1[ii] != castData2[ii])
        {
            std::cerr << "Wideband data at channel " << channel
                      << " has differing data starting at"
                      << " index " << ii << "\n";
            return false;
        }
    }
    return true;
}

bool compareSupportData(const mem::ScopedArray<sys::ubyte>& data1,
                     const mem::ScopedArray<sys::ubyte>& data2,
                     size_t size)
{
    for (size_t ii = 0; ii < size; ++ii)
    {
        if (data1[ii] != data2[ii])
        {
            std::cerr <<  "Support data has differing data starting at"
                      << " index " << ii << "\n";
            return false;
        }
    }
    return true;
}

bool compareWideband(cphd::CPHDReader& reader1,
                     cphd::CPHDReader& reader2,
                     size_t channelsToProcess,
                     size_t numThreads)
{
    bool dataMatches = true;

    const cphd::Wideband& wideband1 = reader1.getWideband();
    const cphd::Wideband& wideband2 = reader2.getWideband();

    mem::ScopedArray<sys::ubyte> cphdData1;
    mem::ScopedArray<sys::ubyte> cphdData2;

    for (size_t ii = 0; ii < channelsToProcess; ++ii)
    {
        const types::RowCol<size_t> dims1(
                reader1.getMetadata().data.getNumVectors(ii),
                reader1.getMetadata().data.getNumSamples(ii));
        const types::RowCol<size_t> dims2(
                reader2.getMetadata().data.getNumVectors(ii),
                reader2.getMetadata().data.getNumSamples(ii));

        if (dims1 == dims2)
        {
            wideband1.read(ii,
                           0, cphd::Wideband::ALL, 0,
                           cphd::Wideband::ALL,
                           numThreads, cphdData1);

            wideband2.read(ii,
                           0, cphd::Wideband::ALL, 0,
                           cphd::Wideband::ALL,
                           numThreads, cphdData2);

            switch (reader1.getMetadata().data.getSignalFormat())
            {
            case cphd::SampleType::RE08I_IM08I:
                if (!compareCPHDData<std::complex<sys::Int8_T> >(
                        cphdData1.get(),
                        cphdData2.get(),
                        dims1.area(),
                        ii))
                {
                    dataMatches = false;
                }
                break;
            case cphd::SampleType::RE16I_IM16I:
                if (!compareCPHDData<std::complex<sys::Int16_T> >(
                        cphdData1.get(),
                        cphdData2.get(),
                        dims1.area(),
                        ii))
                {
                    dataMatches = false;
                }
                break;
            case cphd::SampleType::RE32F_IM32F:
                if (!compareCPHDData<std::complex<float> >(
                        cphdData1.get(),
                        cphdData2.get(),
                        dims1.area(),
                        ii))
                {
                    dataMatches = false;
                }
                break;
            }
        }
        else
        {
            std::cerr << "Data at channel " << ii
                      << " has differing dimensions\n";
            dataMatches = false;
        }
    }

    return dataMatches;
}

bool checkCPHD(std::string pathname1, std::string pathname2, size_t numThreads, std::vector<std::string>& schemaPathname)
{
    cphd::CPHDReader reader1(pathname1, numThreads, schemaPathname);
    cphd::CPHDReader reader2(pathname2, numThreads, schemaPathname);

    // Check metadata
    if (reader1.getMetadata() != reader2.getMetadata())
    {
        std::cerr << "Metadata does not match \n";
        return false;
    }

    // Check pvp block
    if (reader1.getPVPBlock() != reader2.getPVPBlock())
    {
        std::cerr << "PVPBlock does not match \n";
        return false;
    }

    // Check support block
    mem::ScopedArray<sys::ubyte> readPtr1;
    reader1.getSupportBlock().readAll(numThreads, readPtr1);
    mem::ScopedArray<sys::ubyte> readPtr2;
    reader2.getSupportBlock().readAll(numThreads, readPtr2);
    if (!compareSupportData(readPtr1, readPtr2, reader1.getMetadata().data.getAllSupportSize()))
    {
        std::cerr << "SupportBlock does not match \n";
        return false;
    }

    // // Check wideband
    const size_t channelsToProcess = std::min(
            reader1.getMetadata().data.getNumChannels(),
            reader2.getMetadata().data.getNumChannels());
    if (reader1.getMetadata().data.getNumChannels() !=
        reader2.getMetadata().data.getNumChannels())
    {
        std::cerr << "Files contain a differing number of channels "
                  << "comparison will continue but will only look at "
                  << "the first " << channelsToProcess << " channels\n";
        return false;
    }
    //! Only process wideband data if the data types are the same
    if (reader1.getMetadata().data.getSignalFormat() ==
        reader2.getMetadata().data.getSignalFormat())
    {
        const bool cphdDataMatches = compareWideband(reader1,
                                                     reader2,
                                                     channelsToProcess,
                                                     numThreads);
        if (!cphdDataMatches)
        {
            std::cerr << "Wideband data does not match \n";
            return false;
        }
    }
    else
    {
        std::cerr << "Data has differing sample type\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    try
    {
        // Parse the command line
        cli::ArgumentParser parser;
        parser.setDescription("Round trip for a CPHD file.");
        parser.addArgument("-t --threads",
                           "Specify the number of threads to use",
                           cli::STORE,
                           "threads",
                           "NUM")->setDefault(sys::OS().getNumCPUs());
        parser.addArgument("file1", "First pathname", cli::STORE, "file1",
                           "CPHD", 1, 1);
        parser.addArgument("file2", "Second pathname", cli::STORE, "file2",
                           "CPHD", 1, 1);
        parser.addArgument("schema", "Schema pathname", cli::STORE, "schema",
                           "XSD", 1, 1);
        const std::auto_ptr<cli::Results> options(parser.parse(argc, argv));
        const std::string pathname1(options->get<std::string>("file1"));
        const std::string pathname2(options->get<std::string>("file2"));
        const std::string schemaPathname(options->get<std::string>("schema"));
        const size_t numThreads(options->get<size_t>("threads"));

        std::vector<std::string> schemas;
        if (!schemaPathname.empty())
        {
            schemas.push_back(schemaPathname);
        }

        const bool isMatch = checkCPHD(pathname1, pathname2, numThreads, schemas);
        isMatch == true ? std::cout << "CPHD Files match \n" : std::cerr << "CPHD Files do not match \n";
        return 0;
    }
    catch (const except::Exception& ex)
    {
        std::cerr << ex.toString() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception\n";
    }
    return 1;
}