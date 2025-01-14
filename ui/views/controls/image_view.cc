// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"

namespace views {

namespace {

// Returns the pixels for the bitmap in |image| at scale |image_scale|.
void* GetBitmapPixels(const gfx::ImageSkia& img, float image_scale) {
  DCHECK_NE(0.0f, image_scale);
  return img.GetRepresentation(image_scale).sk_bitmap().getPixels();
}

}  // namespace

// static
const char ImageView::kViewClassName[] = "ImageView";

ImageView::ImageView()
    : horizontal_alignment_(CENTER),
      vertical_alignment_(CENTER),
      last_paint_scale_(0.f),
      last_painted_bitmap_pixels_(nullptr) {}

ImageView::~ImageView() {}

void ImageView::SetImage(const gfx::ImageSkia& img) {
  if (IsImageEqual(img))
    return;

  last_painted_bitmap_pixels_ = nullptr;
  gfx::Size pref_size(GetPreferredSize());
  image_ = img;
  scaled_image_ = gfx::ImageSkia();
  if (pref_size != GetPreferredSize())
    PreferredSizeChanged();
  SchedulePaint();
}

void ImageView::SetImage(const gfx::ImageSkia* image_skia) {
  if (image_skia) {
    SetImage(*image_skia);
  } else {
    gfx::ImageSkia t;
    SetImage(t);
  }
}

const gfx::ImageSkia& ImageView::GetImage() const {
  return image_;
}

void ImageView::SetImageSize(const gfx::Size& image_size) {
  image_size_ = image_size;
  PreferredSizeChanged();
}

gfx::Rect ImageView::GetImageBounds() const {
  gfx::Size image_size = GetImageSize();
  return gfx::Rect(ComputeImageOrigin(image_size), image_size);
}

void ImageView::ResetImageSize() {
  image_size_.reset();
}

bool ImageView::IsImageEqual(const gfx::ImageSkia& img) const {
  // Even though we copy ImageSkia in SetImage() the backing store
  // (ImageSkiaStorage) is not copied and may have changed since the last call
  // to SetImage(). The expectation is that SetImage() with different pixels is
  // treated as though the image changed. For this reason we compare not only
  // the backing store but also the pixels of the last image we painted.
  return image_.BackedBySameObjectAs(img) &&
      last_paint_scale_ != 0.0f &&
      last_painted_bitmap_pixels_ == GetBitmapPixels(img, last_paint_scale_);
}

gfx::Size ImageView::GetImageSize() const {
  return image_size_.value_or(image_.size());
}

gfx::Point ImageView::ComputeImageOrigin(const gfx::Size& image_size) const {
  gfx::Insets insets = GetInsets();

  int x = 0;
  // In order to properly handle alignment of images in RTL locales, we need
  // to flip the meaning of trailing and leading. For example, if the
  // horizontal alignment is set to trailing, then we'll use left alignment for
  // the image instead of right alignment if the UI layout is RTL.
  Alignment actual_horizontal_alignment = horizontal_alignment_;
  if (base::i18n::IsRTL() && (horizontal_alignment_ != CENTER)) {
    actual_horizontal_alignment =
        (horizontal_alignment_ == LEADING) ? TRAILING : LEADING;
  }
  switch (actual_horizontal_alignment) {
    case LEADING:
      x = insets.left();
      break;
    case TRAILING:
      x = width() - insets.right() - image_size.width();
      break;
    case CENTER:
      x = (width() - insets.width() - image_size.width()) / 2 + insets.left();
      break;
  }

  int y = 0;
  switch (vertical_alignment_) {
    case LEADING:
      y = insets.top();
      break;
    case TRAILING:
      y = height() - insets.bottom() - image_size.height();
      break;
    case CENTER:
      y = (height() - insets.height() - image_size.height()) / 2 + insets.top();
      break;
  }

  return gfx::Point(x, y);
}

void ImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  OnPaintImage(canvas);
}

void ImageView::SetAccessibleName(const base::string16& accessible_name) {
  accessible_name_ = accessible_name;
}

base::string16 ImageView::GetAccessibleName() const {
  return accessible_name_;
}

void ImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kImage;
  node_data->SetName(accessible_name_);
}

const char* ImageView::GetClassName() const {
  return kViewClassName;
}

void ImageView::SetHorizontalAlignment(Alignment alignment) {
  if (alignment != horizontal_alignment_) {
    horizontal_alignment_ = alignment;
    SchedulePaint();
  }
}

ImageView::Alignment ImageView::GetHorizontalAlignment() const {
  return horizontal_alignment_;
}

void ImageView::SetVerticalAlignment(Alignment alignment) {
  if (alignment != vertical_alignment_) {
    vertical_alignment_ = alignment;
    SchedulePaint();
  }
}

ImageView::Alignment ImageView::GetVerticalAlignment() const {
  return vertical_alignment_;
}

// TODO(crbug.com/890465): Update the duplicate code here and in views::Button.
void ImageView::SetTooltipText(const base::string16& tooltip) {
  tooltip_text_ = tooltip;
  if (accessible_name_.empty())
    accessible_name_ = tooltip_text_;
}

base::string16 ImageView::GetTooltipText() const {
  return tooltip_text_;
}

bool ImageView::GetTooltipText(const gfx::Point& p,
                               base::string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = GetTooltipText();
  return true;
}

gfx::Size ImageView::CalculatePreferredSize() const {
  gfx::Size size = GetImageSize();
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

views::PaintInfo::ScaleType ImageView::GetPaintScaleType() const {
  // ImageView contains an image which is rastered at the device scale factor.
  // By default, the paint commands are recorded at a scale factor slightly
  // different from the device scale factor. Re-rastering the image at this
  // paint recording scale will result in a distorted image. Paint recording
  // scale might also not be uniform along the x & y axis, thus resulting in
  // further distortion in the aspect ratio of the final image.
  // |kUniformScaling| ensures that the paint recording scale is uniform along
  // the x & y axis and keeps the scale equal to the device scale factor.
  // See http://crbug.com/754010 for more details.
  return views::PaintInfo::ScaleType::kUniformScaling;
}

void ImageView::OnPaintImage(gfx::Canvas* canvas) {
  last_paint_scale_ = canvas->image_scale();
  last_painted_bitmap_pixels_ = nullptr;

  gfx::ImageSkia image = GetPaintImage(last_paint_scale_);
  if (image.isNull())
    return;

  gfx::Rect image_bounds(GetImageBounds());
  if (image_bounds.IsEmpty())
    return;

  if (image_bounds.size() != gfx::Size(image.width(), image.height())) {
    // Resize case
    cc::PaintFlags flags;
    flags.setFilterQuality(kLow_SkFilterQuality);
    canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                         image_bounds.x(), image_bounds.y(),
                         image_bounds.width(), image_bounds.height(), true,
                         flags);
  } else {
    canvas->DrawImageInt(image, image_bounds.x(), image_bounds.y());
  }
  last_painted_bitmap_pixels_ = GetBitmapPixels(image, last_paint_scale_);
}

gfx::ImageSkia ImageView::GetPaintImage(float scale) {
  if (image_.isNull())
    return image_;

  const gfx::ImageSkiaRep& rep = image_.GetRepresentation(scale);
  if (rep.scale() == scale)
    return image_;

  if (scaled_image_.HasRepresentation(scale))
    return scaled_image_;

  // Only caches one image rep for the current scale.
  scaled_image_ = gfx::ImageSkia();

  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(rep.pixel_size(), scale / rep.scale());
  scaled_image_.AddRepresentation(gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.sk_bitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    scaled_size.width(), scaled_size.height()),
      scale));
  return scaled_image_;
}

}  // namespace views
