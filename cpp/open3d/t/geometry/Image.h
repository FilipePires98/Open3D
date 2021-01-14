// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "open3d/core/Dtype.h"
#include "open3d/core/Tensor.h"
#include "open3d/core/kernel/UnaryEW.h"
#include "open3d/geometry/Image.h"
#include "open3d/t/geometry/Geometry.h"

namespace open3d {
namespace t {
namespace geometry {

/// \class Image
///
/// \brief The Image class stores image with customizable rols, cols, channels,
/// dtype and device.
class Image : public Geometry {
public:
    /// \enum ColorToIntensityConversionType
    ///
    /// \brief Specifies whether R, G, B channels have the same weight when
    /// converting to intensity. Only used for Image with 3 channels.
    ///
    /// When `Weighted` is used R, G, B channels are weighted according to the
    /// Digital ITU BT.601 standard: I = 0.299 * R + 0.587 * G + 0.114 * B.
    enum class ColorConversionType {
        /// R, G, B channels have equal weights.
        RGBToGrayEqual,
        /// Weighted R, G, B channels: I = 0.299 * R + 0.587 * G + 0.114 * B.
        RGBToGrayWeighted,
    };

    /// \enum FilterType
    ///
    /// \brief Specifies the Image filter type.
    enum class FilterType {
        /// Gaussian filter of size 3 x 3.
        Gaussian3,
        /// Gaussian filter of size 5 x 5.
        Gaussian5,
        /// Gaussian filter of size 7 x 7.
        Gaussian7,
        /// Sobel filter along X-axis.
        Sobel3Dx,
        /// Sobel filter along Y-axis.
        Sobel3Dy
    };

    /// \brief Constructor for image.
    ///
    /// Row-major storage is used, similar to OpenCV. Use (row, col, channel)
    /// indexing order for image creation and accessing. In general, (r, c, ch)
    /// are the preferred variable names for consistency, and avoid using width,
    /// height, u, v, x, y for coordinates.
    ///
    /// \param rows Number of rows of the image, i.e. image height. \p rows must
    /// be non-negative.
    /// \param cols Number of columns of the image, i.e. image width. \p cols
    /// must be non-negative.
    /// \param channels Number of channels of the image. E.g. for RGB image,
    /// channels == 3; for grayscale image, channels == 1. \p channels must be
    /// greater than 0.
    /// \param dtype Data type of the image.
    /// \param device Device where the image is stored.
    Image(int64_t rows = 0,
          int64_t cols = 0,
          int64_t channels = 1,
          core::Dtype dtype = core::Dtype::Float32,
          const core::Device &device = core::Device("CPU:0"));

    /// \brief Construct from a tensor. The tensor won't be copied and memory
    /// will be shared.
    ///
    /// \param tensor: Tensor of the image. The tensor must be contiguous. The
    /// tensor must be 2D (rows, cols) or 3D (rows, cols, channels).
    Image(const core::Tensor &tensor);

    virtual ~Image() override {}

public:
    /// Clear image contents by resetting the rows and cols to 0, while
    /// keeping channels, dtype and device unchanged.
    Image &Clear() override {
        data_ = core::Tensor({0, 0, GetChannels()}, GetDtype(), GetDevice());
        return *this;
    }

    /// Returns true if rows * cols * channels == 0.
    bool IsEmpty() const override {
        return GetRows() * GetCols() * GetChannels() == 0;
    }

public:
    /// Get the number of rows of the image.
    int64_t GetRows() const { return data_.GetShape()[0]; }

    /// Get the number of columns of the image.
    int64_t GetCols() const { return data_.GetShape()[1]; }

    /// Get the number of channels of the image.
    int64_t GetChannels() const { return data_.GetShape()[2]; }

    /// Get dtype of the image.
    core::Dtype GetDtype() const { return data_.GetDtype(); }

    /// Get device of the image.
    core::Device GetDevice() const { return data_.GetDevice(); }

    /// Get pixel(s) in the image. If channels == 1, returns a tensor with shape
    /// {}, otherwise returns a tensor with shape {channels,}. The returned
    /// tensor is a slice of the image's tensor, so when modifying the slice,
    /// the original tensor will also be modified.
    core::Tensor At(int64_t r, int64_t c) const {
        if (GetChannels() == 1) {
            return data_[r][c][0];
        } else {
            return data_[r][c];
        }
    }

    /// Get pixel(s) in the image. Returns a tensor with shape {}.
    core::Tensor At(int64_t r, int64_t c, int64_t ch) const {
        return data_[r][c][ch];
    }

    /// Get raw buffer of the Image data.
    void *GetDataPtr() { return data_.GetDataPtr(); }

    /// Get raw buffer of the Image data.
    const void *GetDataPtr() const { return data_.GetDataPtr(); }

    /// Retuns the underlying Tensor of the Image.
    core::Tensor AsTensor() const { return data_; }

    constexpr static const double SCALE_DEFAULT =
            -std::numeric_limits<double>::max();
    /// Returns an Image with the specified \p dtype.
    /// \param dtype The targeted dtype to convert to.
    /// \param alpha Optional scale value. This is 1./255 for UInt8 ->
    /// Float{32,64}, 1./65535 for UInt16 -> Float{32,64} and 1 otherwise
    /// \param beta Optional shift value. Default 0.
    /// \param copy If true, a new tensor is always created; if false, the copy
    /// is avoided when the original tensor already have the targeted dtype.
    //
    // Use Tensor::To and Tensor operators
    Image ConvertTo(core::Dtype dtype,
                    double scale = SCALE_DEFAULT,
                    double offset = 0.0,
                    bool copy = false) const;

    Image ConvertColor(Image::ColorConversionType cctype);

    /// Function to linearly transform pixel intensities in place.
    /// image = scale * image + offset.
    Image LinearTransform(double scale = 1.0, double offset = 0.0);

    /* Image FlipVertical() const; */
    /* Image FilterHorizontal() const; */
    /* Image Transpose() const; */

    /// Filter image with pre-defined filtering type.
    /* Image FilterFixed(Image::FilterType type) const; */

    /// Filter image with arbitrary dx, dy separable filters.
    /* Image FilterSeparable(const std::vector<double> &dx, */
    /*                       const std::vector<double> &dy) const; */

    /// Function to 2x image downsample using simple 2x2 averaging.
    /* Image Downsample() const; */

    /// Function to dilate 8bit mask map.
    Image Dilate(int half_kernel_size = 1) const;

    /// Compute min 2D coordinates for the data (always {0, 0}).
    core::Tensor GetMinBound() const {
        return core::Tensor::Zeros({2}, core::Dtype::Int64);
    };

    /// Compute max 2D coordinates for the data ({rows, cols}).
    core::Tensor GetMaxBound() const {
        return core::Tensor(std::vector<int64_t>{GetRows(), GetCols()}, {2},
                            core::Dtype::Int64);
    };

    /// Create from a legacy Open3D Image.
    static Image FromLegacyImage(
            const open3d::geometry::Image &image_legacy,
            const core::Device &Device = core::Device("CPU:0"));

    /// Convert to legacy Image type.
    open3d::geometry::Image ToLegacyImage() const;

    /// Text description
    std::string ToString() const;

protected:
    /// Internal data of the Image, represented as a contiguous 3D tensor of
    /// shape {rols, cols, channels}. Image properties can be obtained from the
    /// tensor.
    core::Tensor data_;
};

}  // namespace geometry
}  // namespace t
}  // namespace open3d
