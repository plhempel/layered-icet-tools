/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2021 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef __IceTDevGL3Image_h
#define __IceTDevGL3Image_h

#include <IceTGL3.h>
#include <IceTDevImage.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

ICET_GL3_EXPORT void icetGL3GetRenderedBufferImage(IceTImage target_image,
                                                   IceTInt *rendered_viewport,
                                                   IceTInt *target_viewport);

ICET_GL3_EXPORT IceTSparseImage icetGL3GetCompressedRenderedBufferImage(
        IceTInt *rendered_viewport,
        IceTInt *target_viewport,
        IceTSizeType tile_width,
        IceTSizeType tile_height);

#ifdef __cplusplus
}
#endif

#endif /*__IceTDevGL3Image_h*/
