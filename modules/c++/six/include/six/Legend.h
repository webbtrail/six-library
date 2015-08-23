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

#ifndef __SIX_LEGEND_H__
#define __SIX_LEGEND_H__

#include <vector>

#include <sys/Conf.h>
#include <types/RowCol.h>
#include <mem/ScopedCloneablePtr.h>
#include <six/Types.h>

namespace six
{
/*
 * This is a legend associated with a SIDD product
 * See section 2.4.3 of SIDD Volume 2
 *
 * TODO: Have started adding this in NITFWriteControl.cpp - see legend section
 */
struct Legend
{
public:
    Legend() :
        mType(PixelType::NOT_SET),
        mLocation(0, 0),
        mDims(0, 0)
    {
    }

    // Resizes 'mImage' to match
    void setDims(const types::RowCol<size_t>& dims)
    {
        mDims = dims;
        mImage.resize(dims.row * dims.col);
    }

    // TODO: For now, only RGB8LU is supported
	PixelType mType;

	types::RowCol<size_t> mLocation;

	types::RowCol<size_t> mDims;
	std::vector<sys::ubyte> mImage;
	mem::ScopedCloneablePtr<LUT> mLUT;
};
}

#endif