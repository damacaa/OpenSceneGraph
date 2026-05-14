#pragma once
#include <osgDB/Options>
#include <osgDB/ReaderWriter>
#include <osgUtil/Optimizer>
#include <string>

struct ExportSettings
{
    enum Format
    {
        FMT_OSGB = 0,
        FMT_OSG = 1,
        FMT_OSGT = 2
    };
    enum Compression
    {
        COMP_NONE = 0,
        COMP_ZLIB_FAST = 1,
        COMP_ZLIB = 2,
        COMP_ZLIB_BEST = 3
    };
    enum ImageMode
    {
        IMG_INLINE = 0,
        IMG_INCLUDE_FILE = 1,
        IMG_EXTERNAL = 2
    };
    enum Scale
    {
        SCALE_0001 = 0,
        SCALE_1 = 1,
        SCALE_1000 = 2
    };
    enum Axis
    {
        AXIS_NONE = 0,
        AXIS_Y2Z = 1,
        AXIS_Z2Y = 2
    };

    Format format = FMT_OSGB;
    Compression compression = COMP_NONE;
    ImageMode imageMode = IMG_INLINE;
    Scale scale = SCALE_1;
    Axis axis = AXIS_NONE;
    bool mergeGeom = false;
    bool shareState = false;
    bool tristrip = false;
    bool flattenTransforms = false;
    bool indexMesh = false;
    bool removeRedundant = false;

    std::string extension() const
    {
        if (format == FMT_OSG)
            return ".osg";
        if (format == FMT_OSGT)
            return ".osgt";
        return ".osgb";
    }

    std::string compressorName() const
    {
        if (format != FMT_OSGB)
            return "";
        switch (compression)
        {
        case COMP_ZLIB_FAST:
            return "zlib_fast";
        case COMP_ZLIB:
            return "zlib";
        case COMP_ZLIB_BEST:
            return "zlib_best";
        default:
            return "";
        }
    }

    osgDB::ReaderWriter::Options *makeOptions() const
    {
        std::string s;
        std::string comp = compressorName();
        if (!comp.empty())
            s += "Compressor " + comp + " ";
        if (imageMode == IMG_INLINE)
            s += "WriteImageHint=IncludeData ";
        else if (imageMode == IMG_INCLUDE_FILE)
            s += "WriteImageHint=IncludeFile ";
        else
            s += "WriteImageHint=UseExternal ";
        return new osgDB::ReaderWriter::Options(s);
    }

    float scaleFactor() const
    {
        switch (scale)
        {
        case SCALE_0001:
            return 0.001f;
        case SCALE_1000:
            return 1000.0f;
        default:
            return 1.0f;
        }
    }

    bool needsTransform() const { return scale != SCALE_1 || axis != AXIS_NONE; }

    unsigned int optimizerFlags() const
    {
        unsigned int f = 0;
        if (mergeGeom)
            f |= osgUtil::Optimizer::MERGE_GEOMETRY;
        if (shareState)
            f |= osgUtil::Optimizer::SHARE_DUPLICATE_STATE;
        if (tristrip)
            f |= osgUtil::Optimizer::TRISTRIP_GEOMETRY;
        if (flattenTransforms)
            f |= osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS;
        if (indexMesh)
            f |= osgUtil::Optimizer::INDEX_MESH;
        if (removeRedundant)
            f |= osgUtil::Optimizer::REMOVE_REDUNDANT_NODES;
        return f;
    }
};
